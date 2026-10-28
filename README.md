# BlueRetro — PlayStation 1 Custom Edition (v0.7.4)

<p align="center">
  <img src="https://raw.githubusercontent.com/darthcloud/BlueRetro/refs/heads/master/static/PNGs/BRE_Logo_Color_Outline.png" alt="BlueRetro Logo" width="300">
</p>

<p align="center">
  <a href="https://github.com/Schickacka/BlueRetro/releases"><img src="https://img.shields.io/github/v/release/Schickacka/BlueRetro?color=0078D7&label=Release&logo=github" alt="GitHub Release"></a>
  <a href="https://github.com/Schickacka/BlueRetro/actions"><img src="https://img.shields.io/github/actions/workflow/status/Schickacka/BlueRetro/build.yml?branch=master&label=CI%20Build&logo=githubactions" alt="Build Status"></a>
  <img src="https://img.shields.io/badge/Platform-Sony%20PlayStation%201-003791?logo=playstation&logoColor=white" alt="Platform">
  <img src="https://img.shields.io/badge/Hardware-ESP32%20DevKit%20V1-E7352C?logo=espressif&logoColor=white" alt="ESP32">
  <img src="https://img.shields.io/badge/Bluetooth-BR%2FEDR%20%2B%20BLE%20OTA-0078D7?logo=bluetooth&logoColor=white" alt="Bluetooth">
  <img src="https://img.shields.io/badge/License-Apache%202.0-blue.svg" alt="License">
</p>

---

## Visão Geral

O **BlueRetro — PlayStation 1 Custom Edition** é um firmware customizado de alta precisão para receptores Bluetooth baseados em **ESP32 (DevKit V1 / ESP-WROOM-32)** voltado ao console **Sony PlayStation 1** (compatível com todas as revisões de hardware: PU-7, PU-8, PU-18, PU-20, PU-22, PU-23 e PSOne PM-41).

Baseado no projeto de Jacques Gagnon ([BlueRetro](https://github.com/darthcloud/BlueRetro)), esta edição foi desenvolvida e mantida por **Schickacka** com o objetivo de implementar calibrações eletromecânicas fiéis às especificações do controle DualShock original (SCPH-1200), robustez de concorrência em tempo real e estabilidade total de feedback em controladores sem fio modernos (DualShock 4, DualSense, Xbox Wireless, Switch Pro e 8BitDo).

---

## Arquitetura e Configurações de Hardware

### 1. Limiar Eletromecânico do Atuador de Baixa Frequência (0x40 / 64)
* **Comportamento do Hardware Original:** No controle físico DualShock 1 (SCPH-1200), o motor esquerdo (baixa frequência) é composto por um contrapeso excêntrico de ferro maciço alimentado pela linha de 7.6V do console. Devido ao atrito estático e à inércia mecânica desse conjunto, comandos com sinal PWM inferior a `0x40` (64 em decimal) não produzem torque de partida suficiente para girar o motor físico, permanecendo completamente imóvel no hardware com fio original.
* **Compatibilidade com Controles Modernos:** Jogos de PlayStation 1 frequentemente emitem valores residuais contínuos de baixa amplitude (`< 0x40`) nos registradores de feedback como ruído de fundo ou nível de linha de base. Em controladores sem fio modernos equipados com atuadores de 3.7V (que possuem limiar de partida muito baixo), esses valores residuais faziam o contrapeso girar de forma contínua em momentos nos quais o controle com fio original permanecia em repouso absoluto.
* **Calibração Implementada:** Comandos de rotação do motor pesado com valores inferiores a `0x40` são tratados estritamente como repouso (`0x00`), acionando o motor apenas quando a intensidade exigida for igual ou superior a 64:
  $$\text{Power}_{\text{LF}} = \begin{cases} \text{raw}, & \text{se } \text{raw} \ge 0x40 \\ 0, & \text{se } \text{raw} < 0x40 \end{cases}$$
  Isso restaura a resposta tátil original do SCPH-1200 em toda a biblioteca do sistema, permitindo que efeitos modulados em pulso (como batimentos cardíacos, vibrações de advertência e passos) vibrem com impacto firme e cessem com silêncio cirúrgico entre as pulsações.
* **Ativação do Motor de Alta Frequência:** O motor direito (leve/alta frequência) é tratado como sinal binário digital, no qual qualquer byte não nulo aciona saturação máxima (`0xFF`), garantindo resposta tátil nítida e imediata.

### 2. Transmissão Contínua de Feedback em Alta Frequência (20 ms / 50 Hz)
* **Temporizador Watchdog dos Controles Sem Fio:** Os controladores Bluetooth modernos (DualShock 4, DualSense, Xbox Wireless, Switch Pro) possuem um temporizador interno de segurança (*watchdog*) em seus firmwares de aproximadamente 200 ms a 250 ms. Se os motores estiverem ativos e nenhum novo relatório HID Bluetooth for recebido dentro dessa janela, o controle atenua ou desliga a alimentação dos atuadores por proteção térmica e conservação de bateria.
* **Sustentação de Eventos Hápticos Prolongados:** Em situações de vibração contínua e constante (como disparos sustentados de armas automáticas, aceleração contínua em zebras de pista ou explosões de longa duração), o console mantém os mesmos valores nos registradores de comando quadro a quadro. Na ausência de variação nos dados, a rotina de feedback pausava a transmissão por 300 ms, gerando solavancos e microcortes cíclicos causados pelo desarmamento do watchdog interno do controle.
* **Calibração Implementada:** A rotina de transmissão Bluetooth (`bt_fb_task`) monitora atomicamente o estado ativo de vibração (`BT_DEV_RUMBLE_ON`). Enquanto qualquer controle conectado estiver vibrando, os pacotes de feedback são retransmitidos a cada **20 ms (50 Hz)** — taxa rigorosamente síncrona com os quadros de vídeo do hardware do PlayStation 1. Isso mantém o watchdog do controle continuamente alimentado, garantindo uma resposta de vibração uniforme e reduzindo microcortes durante toda a sustentação do comando.

### 3. Interrupção Imediata de Vibração em Pausa e Menus (Comando 0x43)
* Quando o jogo entra em pausa ou acessa telas de configuração/inventário, o console chaveia a comunicação serial para o comando `0x43`. O driver intercepta imediatamente essa transição e emite um pacote de parada forçada com comprimento zero (`data_len = 0`), desligando os motores de forma instantânea sem resíduos de rotação.

### 4. Limiar Estendido de Inatividade do Barramento SPI (3,0 Segundos)
* Durante transições de tela, telas de carregamento do CD-ROM ou reprodução de vídeos em tela cheia (FMV), determinados jogos suspendem temporariamente a comunicação com as portas de controle por mais de um segundo. O limiar de inatividade para reinicialização (`PS_IDLE_RESET_US`) foi estendido para **3,0 segundos**, prevenindo que pausas de leitura do disco desarmem o modo analógico, enquanto preserva a capacidade de detectar com precisão quando o console foi reiniciado pelo botão físico de Reset.

### 5. Sincronização Imediata de Modo Analógico e Iluminação Balanceada
* **Sincronização na Conexão:** Ao ligar o controle no meio de uma partida na qual o console já esteja operando em modo Analógico (`0x73`), o controle entra diretamente em Analógico e acende a barra de luz na cor correspondente, sem exigir reinicialização do console ou alternância manual.
* **Iluminação Noturna a 50%:** A barra de luz RGB opera com luminosidade equilibrada para evitar ofuscamento em ambientes com baixa iluminação:
  * **Modo Analógico Ativo:** Vermelho clássico do SCPH-1200 calibrado com atenuação de 15% (`#6D0000`).
  * **Modo Digital:** DualShock 4 em cinza neutro (`#949190`); DualSense com a lightbar apagada (`0x000000`) e apenas os pontos de jogador acesos.

### 6. Sincronização Atômica entre Núcleos (Dual-Core)
* O firmware distribui suas cargas entre os dois núcleos do ESP32: o **Core 1** é responsável pelo driver baremetal SPI em tempo real (`ps_spi.c`), enquanto o **Core 0** gerencia a pilha de protocolos Bluetooth e o sistema operacional FreeRTOS (`host.c`).
* Todas as variáveis compartilhadas entre a ISR baremetal e as tarefas de background são declaradas como `volatile` e protegidas com barreiras explícitas de sincronização de memória (`__sync_synchronize()`), assegurando que as transições de estado do controle sejam visíveis de imediato em ambos os núcleos.

---

## Mapeamento de Pinos (Pinout)

Configuração padrão dos pinos de conexão entre a placa **ESP32 DevKit V1** e as portas do console:

| Sinal PlayStation | Pino do Console | GPIO ESP32 | Descrição e Observações |
| :--- | :---: | :---: | :--- |
| **P1 DATA (MISO)** | **Pino 1** | **GPIO 19** | Dados seriais enviados do controle para o console (Porta 1) |
| **P1 COMMAND (MOSI)**| **Pino 2** | **GPIO 23** | Comandos enviados do console para o controle (Porta 1) |
| **P1 ATT / DTR**   | **Pino 6** | **GPIO 34** | Linha de seleção da Porta 1 (requer resistor pull-up externo de 10k) |
| **P1 CLOCK (SCK)** | **Pino 7** | **GPIO 18** | Linha de clock de sincronização (Porta 1) |
| **P1 ACK / DSR**   | **Pino 9** | **GPIO 21** | Pulso de confirmação de byte recebido (Porta 1) |
| **P2 DATA (MISO)** | **Pino 1** | **GPIO 22** | Dados seriais da Porta 2 |
| **P2 COMMAND (MOSI)**| **Pino 2** | **GPIO 25** | Comandos seriais da Porta 2 |
| **P2 ATT / DTR**   | **Pino 6** | **GPIO 5**  | Linha de seleção da Porta 2 |
| **P2 CLOCK (SCK)** | **Pino 7** | **GPIO 26** | Linha de clock da Porta 2 |
| **P2 ACK / DSR**   | **Pino 9** | **GPIO 27** | Pulso de confirmação da Porta 2 |
| **Alimentação 3.5V**| **Pino 5** | **Pino 3.3V**| Alimentação lógica do barramento serial |
| **GND (Terra)**    | **Pino 4** | **GND**     | Referência de terra comum |
| **LED Analógico Porta 1** | **GPIO 12** | Nível alto (3.3V) quando a Porta 1 opera em Modo Analógico |
| **LED Analógico Porta 2** | **GPIO 15** | Nível alto (3.3V) quando a Porta 2 opera em Modo Analógico |
| **LED Onboard (Status BT)**| **GPIO 2** | LED azul integrado da placa (indica aguardo de conexão / atividade) |

---

## Diretrizes de Alimentação

1. **Alimentação Direta pela Linha de 3.5V (Pino 5 do Console):**
   * Deve ser conectada diretamente ao pino **`3.3V`** da placa ESP32.
   * **Importante:** Não conecte a linha de 3.5V ao pino `VIN` / `5V` do ESP32. O pino VIN passa por um regulador linear LDO (como o AMS1117-3.3), cuja queda de tensão interna (*dropout*) exige no mínimo 4.5V a 5.0V na entrada. A alimentação de 3.5V no VIN causa reinicializações espontâneas por subtensão (*brownout reset*).
2. **Alimentação via Conversor Step-Down DC-DC:**
   * Alternativamente, conecte um mini módulo regulador Buck DC-DC (ex.: MP1584EN) ao **Pino 3 (7.6V)** do conector do console.
   * Ajuste o conversor para fornecer **5.0V regulados** e conecte a saída ao pino **`VIN` (5V)** da placa ESP32.

---

## Instalação e Atualização do Firmware

### Método 1: Atualização Sem Fio via Bluetooth (OTA - Recomendado)
Se o BlueRetro já estiver instalado no console, o procedimento pode ser realizado sem abrir o videogame e sem cabos:

> [!NOTE]
> O script de atualização sem fio (`ble_ota_update.py`) requer a biblioteca Python `bleak` (`pip install bleak`).

1. Baixe o pacote **[`BlueRetro_PS1_Custom_v0.7.4_Package.zip`](https://github.com/Schickacka/BlueRetro/releases)** na aba Releases.
2. Ligue o PlayStation 1 com os controles Bluetooth desligados.
3. Execute o arquivo **`flash_bluetooth_ota.bat`** (Windows).
4. O script localizará o dispositivo via Bluetooth LE e efetuará a gravação em aproximadamente 50 segundos. O receptor reiniciará automaticamente com o firmware atualizado.

*Alternativa via Navegador:* Acesse **[blueretro.io/ota.html](https://blueretro.io/ota.html)** no Chrome ou Edge, conecte-se ao BlueRetro e envie o arquivo `BlueRetro_hw1_playstation.bin`.

### Método 2: Gravação via Cabo USB (Windows)
1. Conecte o ESP32 ao computador via cabo micro-USB.
2. Extraia o conteúdo de **`BlueRetro_PS1_Custom_v0.7.4_Package.zip`**.
3. Execute **`flash_windows.bat`** e informe a porta serial correspondente (exemplo: `COM3`, `COM4`, `COM6`).

### Método 3: Gravação Manual via Linha de Comando (esptool)
```bash
python -m esptool --chip esp32 --port COMx -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 BlueRetro_hw1_playstation.bin
```

---

## Compatibilidade de Hardware e Controles

### Controles Homologados
* **Sony DualShock 4** (todas as revisões de hardware: CUH-ZCT1 e CUH-ZCT2).
* **Sony DualSense (PS5)** (atuadores de precisão mapeados para baixa e alta frequência).
* **Nintendo Switch Pro Controller** e Joy-Cons.
* **Xbox Wireless Controller** (Modelos 1708 com Bluetooth, Series X/S 1914).
* **Linha 8BitDo** (Pro 2, Ultimate Bluetooth, SN30 Pro, M30).

### Padrões de Feedback e Comportamento em Jogos
O firmware foi projetado e calibrado para atender aos padrões de comunicação e feedback utilizados pela biblioteca do PlayStation (sujeito à validação física em hardware real):
* **Efeitos de Pulso e Modulação:** Resposta de motores pesados com repouso verdadeiro nos intervalos de baixa intensidade (como batimentos cardíacos, avisos de proximidade e alertas de status).
* **Vibração Sustentada Contínua:** Disparos prolongados de armas automáticas, aceleração contínua e superfícies acidentadas reduzindo microcortes por watchdog.
* **Transições de Cena e Vídeos:** Preservação do modo analógico durante pausas de leitura em FMVs e telas de transição.
* **Resposta de Impacto:** Acionamento agudo em golpes, impactos rápidos e colisões.

---

## Compilação a Partir do Código-Fonte

Este repositório possui fluxo automatizado de integração contínua (CI) via **GitHub Actions** que gera os binários a cada commit na branch `master`. Para compilação local utilizando o framework **ESP-IDF v5.5**:

```bash
# Clonagem recursiva obrigatória para obtenção de submódulos
git clone --recursive https://github.com/Schickacka/BlueRetro.git
cd BlueRetro

# Gerar arquivo de versão do firmware customizado
echo "v0.7.4 ps1-custom" > version.txt

# Carregar arquivo de configuração para PlayStation
cp configs/hw1/playstation sdkconfig

# Compilar binários
BR_HW=_hw1 BR_SYS=_playstation idf.py reconfigure build
```

---

## Licença e Créditos

* **BlueRetro:** Copyright (c) 2019-2025, Jacques Gagnon. Distribuído sob a licença **Apache License 2.0**.
* **PlayStation 1 Custom Edition:** Desenvolvido e mantido por **Schickacka**.
* Todas as alterações de código e adições arquiteturais estão documentadas nos arquivos modificados em conformidade com a seção 4(b) da licença Apache 2.0.
