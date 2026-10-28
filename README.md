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

O **BlueRetro — PlayStation 1 Custom Edition** é um firmware customizado de alta precisão para receptores Bluetooth baseados em **ESP32 (DevKit V1 / ESP-WROOM-32)** voltado ao console **Sony PlayStation 1** (testado em PU-8 late 1-658-467-21; a pinagem é a mesma nas demais revisões).

Baseado no projeto de Jacques Gagnon ([BlueRetro](https://github.com/darthcloud/BlueRetro)), esta edição foi desenvolvida e mantida por **Schickacka** com o objetivo de implementar calibrações eletromecânicas fiéis às especificações do controle DualShock original (SCPH-1200), robustez de concorrência em tempo real e estabilidade total de feedback em controladores sem fio modernos (DualShock 4 e DualSense).

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
* **Temporizador Watchdog dos Controles Sem Fio:** Os controladores Bluetooth modernos (DualShock 4 e DualSense) possuem um temporizador interno de segurança (*watchdog*) em seus firmwares de aproximadamente 200 ms a 250 ms. Se os motores estiverem ativos e nenhum novo relatório HID Bluetooth for recebido dentro dessa janela, o controle atenua ou desliga a alimentação dos atuadores por proteção térmica e conservação de bateria.
* **Sustentação de Eventos Hápticos Prolongados:** Em situações de vibração contínua e constante (como disparos sustentados de armas automáticas, aceleração contínua em zebras de pista ou explosões de longa duração), o console mantém os mesmos valores nos registradores de comando quadro a quadro. Na ausência de variação nos dados, a rotina de feedback pausava a transmissão por 300 ms, gerando solavancos e microcortes cíclicos causados pelo desarmamento do watchdog interno do controle.
* **Calibração Implementada:** A rotina de transmissão Bluetooth (`bt_fb_task`) monitora atomicamente o estado ativo de vibração (`BT_DEV_RUMBLE_ON`). Enquanto qualquer controle conectado estiver vibrando, os pacotes de feedback são retransmitidos a cada **20 ms (50 Hz nominais)**, por temporizador de software. Não há sincronismo com o vídeo do console: o PS1 opera a 59,94 Hz (NTSC/PAL-M) ou 50 Hz (PAL); o período de 20 ms serve apenas para manter vivo o watchdog interno do controle.

### 3. Interrupção Imediata de Vibração em Pausa e Menus (Comando 0x43)
* Quando o jogo entra em pausa ou acessa telas de configuração/inventário, o console chaveia a comunicação serial para o comando `0x43`. O driver intercepta imediatamente essa transição e emite um pacote de parada forçada com comprimento zero (`data_len = 0`), desligando os motores de forma instantânea sem resíduos de rotação.

### 4. Limiar Estendido de Inatividade do Barramento SPI (3,0 Segundos)
* Durante transições de tela, telas de carregamento do CD-ROM ou reprodução de vídeos em tela cheia (FMV), determinados jogos suspendem temporariamente a comunicação com as portas de controle por mais de um segundo. O limiar de inatividade para reinicialização (`PS_IDLE_RESET_US`) foi estendido para **3,0 segundos**, prevenindo que pausas de leitura do disco desarmem o modo analógico, enquanto preserva a capacidade de detectar com precisão quando o console foi reiniciado pelo botão físico de Reset.

### 5. Sincronização Imediata de Modo Analógico e Iluminação Balanceada
* **Sincronização na Conexão:** Ao ligar o controle no meio de uma partida na qual o console já esteja operando em modo Analógico (`0x73`), o controle entra diretamente em Analógico e acende a barra de luz na cor correspondente, sem exigir reinicialização do console ou alternância manual.
* **Cores padrão do build de release:**
  * **Modo Analógico:** vermelho `#6D0000` (R=0x6D, ≈43% do vermelho máximo).
  * **Modo Digital:** DualShock 4 em cinza `#949190`; DualSense com a lightbar
    apagada (`0x000000`) e apenas os pontos de jogador acesos.
* **Modo noturno (opcional):** só existe com `CONFIG_BR_PS1_NIGHT_MODE=y`
  (padrão `n`). Quando ligado, divide as componentes RGB por 2 e usa o brilho
  mínimo do DS4/DS5.

### 6. Sincronização Atômica entre Núcleos (Dual-Core)
* O firmware distribui suas cargas entre os dois núcleos do ESP32: o **Core 1** é responsável pelo driver baremetal SPI em tempo real (`ps_spi.c`), enquanto o **Core 0** gerencia a pilha de protocolos Bluetooth e o sistema operacional FreeRTOS (`host.c`).
* As variáveis compartilhadas entre a ISR e as tarefas usam `volatile` **mais**
  barreiras (`__sync_synchronize()`) ou operações atômicas (`atomic_cas`) nos
  pontos de publicação de estado. `volatile` sozinho não garante atomicidade de
  campos multi-byte; por isso os campos de estado são de 8/32 bits alinhados e
  as transições críticas são publicadas com barreira explícita.

### 7. Desligamento Automático por Inatividade (10 min)

* Um controle Bluetooth sem botão pressionado, com sticks centrados e sem rumble
  ativo por **10 minutos** é desconectado automaticamente
  (`CONFIG_BR_INACTIVITY_TIMEOUT_MIN`; `0` desativa).
* O console vê o controle desaparecer: jogos que pausam ao perder o controle vão
  pausar. Basta ligar o controle de novo (botão PS/Home) para reconectar.
* Ajuste em `main/Kconfig.projbuild` → menu **PS1 Custom Edition** →
  "Controller inactivity timeout in minutes".

---

## Mapeamento de Pinos (Pinout)

Configuração padrão dos pinos de conexão entre a placa **ESP32 DevKit V1** e as portas do console:

| Sinal PlayStation | Pino do Console | GPIO ESP32 | Observação |
| :--- | :---: | :---: | :--- |
| **P1 DATA (MISO)** | **Pino 1** | **GPIO 19** | ESP32 → console |
| **P1 COMMAND (MOSI)** | **Pino 2** | **GPIO 32** | Console → ESP32 |
| **P1 ATT / DTR** | **Pino 6** | **GPIO 34** | Entrada; **pull-up externo de 10 kΩ obrigatório** (GPIO34 não tem pull-up interno) |
| **P1 CLOCK (SCK)** | **Pino 7** | **GPIO 33** | Console → ESP32 |
| **P1 ACK / DSR** | **Pino 9** | **GPIO 21** | ESP32 → console |
| **P2 DATA (MISO)** | **Pino 1** | **GPIO 22** | ESP32 → console |
| **P2 COMMAND (MOSI)** | **Pino 2** | **GPIO 27** | Console → ESP32 |
| **P2 ATT / DTR** | **Pino 6** | **GPIO 5** | Entrada com pull-up interno |
| **P2 CLOCK (SCK)** | **Pino 7** | **GPIO 26** | Console → ESP32 |
| **P2 ACK / DSR** | **Pino 9** | **GPIO 25** | ESP32 → console |
| **GND (Terra)** | **Pino 4** | **GND** | Referência comum |
| **LED Analógico Porta 1** | — | **GPIO 12** | Nível alto em modo analógico |
| **LED Analógico Porta 2** | — | **GPIO 15** | Nível alto em modo analógico |
| **LED Onboard (Status BT)** | — | **GPIO 2** | LED azul da placa |

> **Alimentação:** não use o pino 5 (3,5 V). Veja "Diretrizes de Alimentação".
> A fonte da verdade dos GPIOs é `main/wired/ps_spi.c` (linhas 97–108); se a
> tabela e o código divergirem, o código ganha.

---

## Diretrizes de Alimentação

> **Nunca alimente o ESP32 pelo pino 5 (3,5 V) do conector do controle.**
> Essa linha vem da fonte do console, é compartilhada com a lógica das portas de
> controle e do memory card e é protegida por fusível. O ESP32 pede picos de
> centenas de mA (rádio Bluetooth em transmissão e calibração de RF no boot) que
> essa linha não foi dimensionada para fornecer: a tensão afunda, causando
> *brownout reset*, e a sobrecarga sostenida abre o fusível — o console perde as
> portas com fio. Além disso, não há regulador nem proteção entre o pino 5 e o
> rail de 3,3 V do ESP32, e 3,5 V está no limite superior da faixa de 3,0–3,6 V
> do chip, sem margem para transitórios.

### Fonte correta: pino 3 (7,6 V) + conversor buck DC-DC

1. Ligue um buck ajustável (ex.: MP1584EN) no **pino 3 (7,6 V)** e ajuste a
   saída para **5,0 V** (capacidade ≥ 1 A).
2. Ligue a saída do buck no pino **`VIN` / `5V`** do ESP32. **Não** ligue no pino
   `3.3V`: o regulador da placa (AMS1117) precisa de pelo menos 4,5 V na entrada.
   Em placas DevKitC com AMS1117 (máximo 18 V), o próprio upstream aceita 8 V
   direto no `VIN`; o buck é preferível porque o LDO dissipa (8 − 5) × I e esquenta.
3. Instale **470 µF + 100 nF** junto ao ESP32 e um **fusível de 1 A** no ramo de
   7,6 V, o mais próximo possível do ponto de derivação.
4. O GND do console (pino 4) e o GND do ESP32 devem ser o mesmo nó.

> O 7,6 V do conector é a linha **não chaveada** na maioria das revisões: com o
> console desligado o ESP32 continua energizado. Se você precisa que ele desligue
> junto com o console, use o pino 5 (3,5 V) **apenas como sinal de enable** de um
> MOSFET / load switch de alta impedância — nunca como fonte de energia.
>
> O pino 5 pode ficar **desconectado**: ele é saída da console (alimenta a lógica
> do controle original), não entrada. O ESP32 não precisa dele para nada.

### Alternativa: 5 V externo (USB)

Alimente o ESP32 por USB (5 V) e ligue ao console apenas o GND e as linhas de
sinal. É a opção mais segura para bancada e para quem não quer mexer na fonte.

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

### Controles Testados
* **Sony DualShock 4** — oficial e clones/paralelos (CUH-ZCT1 e CUH-ZCT2).
* **Sony DualSense (PS5)** — atuadores de precisão mapeados para baixa e alta frequência.

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

> Se você baixou o `Source.zip` (e não clonou com `--recursive`), rode
> `./scripts/fetch_liblfds.sh` antes do build: o zip não traz o submódulo
> `components/queue_bss/liblfds`.

---

## Licença e Créditos

* **BlueRetro:** Copyright (c) 2019-2025, Jacques Gagnon. Distribuído sob a licença **Apache License 2.0**.
* **PlayStation 1 Custom Edition:** Desenvolvido e mantido por **Schickacka**.
* Todas as alterações de código e adições arquiteturais estão documentadas nos arquivos modificados em conformidade com a seção 4(b) da licença Apache 2.0.
