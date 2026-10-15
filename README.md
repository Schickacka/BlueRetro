# BlueRetro — PlayStation 1 Custom Edition (v0.2)

Firmware customizado de alta performance para receptor Bluetooth BlueRetro voltado ao PlayStation 1 (ESP32 DevKit). Desenvolvido por **Schickacka**, baseado no projeto de engenharia reversa e emulação sem fio de Jacques Gagnon ([BlueRetro](https://github.com/darthcloud/BlueRetro)).

---

## 1. Visão Geral das Modificações da Versão 0.2

Esta edição foi projetada para resolver limitações do BlueRetro clássico em instalações no PlayStation 1 (consoles PU-7, PU-8, PU-18, PU-20, PU-22, PU-23 e PSOne PM-41), com foco em controles **DualShock 4** e **DualSense (PS5)**:

* **Sincronização Atômica entre Núcleos (Core 1 ↔ Core 0):**
  * Campos de estado (`dev_id`, `pend_dev_id`, `rumble_*`, `analog_btn`, `last_spi_time`) declarados como `volatile`.
  * Barreiras de memória explícitas (`__sync_synchronize()`) em todas as transições de estado, garantindo coerência de cache e eliminando condições de corrida entre a ISR baremetal do Core 1 e as tarefas do Core 0.
* **Tratamento Robusto de Comandos SPI e Fallback Neutro:**
  * Comandos desconhecidos ou não suportados pelo PlayStation não geram pulso falso de ACK (`DSR`), retornando nível neutro High-Z (`0xFF`) e prevenindo travamentos de protocolo no console.
  * Proteção contra transbordamento de buffer (*boundary check*) em todas as montagens de resposta em `tx_buf`.
* **Filtro de Ruído na Linha de Seleção (DTR / ATT):**
  * Confirmação de amostragem em dois estágios para o pino de seleção (GPIO 34), prevenindo falsos disparos causados por acoplamento capacitivo das transições de clock (SCK).
* **Detecção Inteligente de Reset do Console (Caso *Resident Evil 3*):**
  * Se o barramento SPI silenciar por mais de 3,0 segundos (`PS_IDLE_RESET_US`) — indicando que o console foi reiniciado fisicamente pelo botão RESET —, o adaptador reverte automaticamente para o modo Digital (`0x41`), apaga o LED analógico e reajusta o feedback do controle, prevenindo o bloqueio do jogo na tela de avisos.
* **Sincronização Imediata de LED na Conexão:**
  * Se um controle Bluetooth for conectado enquanto o console já estiver rodando um jogo em modo Analógico (`0x73`), o controle assume imediatamente a cor vermelha (estilo SCPH-1200) ao término da animação de pareamento.
* **Padrão de LED Onboard Otimizado (GPIO 2):**
  * O ciclo de piscadas agora é dimensionado dinamicamente para comportar todos os jogadores conectados sem suprimir piscadas.
  * O roteamento de matriz GPIO do LEDC é acionado exclusivamente na transição de estado, poupando ciclos de CPU da tarefa `sys_mgr_task`.
* **Feedback Háptico na Conexão (Identificação por Jogador):**
  * Ao parear, o controle vibra em pulsos curtos e ágeis de 500 ms com intervalos de 300 ms indicando o número da porta (1 pulso para Player 1, 2 pulsos para Player 2, etc.).
* **Modo Noturno com Brilho a 50%:**
  * Barra de luz calibrada para iluminação confortável em ambientes escuros:
    * Analógico (SCPH-1200): Vermelho suave (`R=128`).
    * Digital: Cinza/Branco ambiente (`RGB=16, 16, 16`).

---

## 2. Esquema Elétrico e Pinagem (Hardware Pinout)

> **Importante:** A pinagem abaixo reflete estritamente as definições do driver baremetal de hardware em `ps_spi.c`. Para instalações de duas portas (Player 1 e Player 2), cada porta possui linhas dedicadas para permitir operação concorrente independente.

### 2.1. Conector de Controles do PlayStation 1

```text
               [ 9   7   6 ]   [ 5   4   3 ]   [ 2   1 ]  (Porta do Console PS1)
               -----------------------------------------
```

| Pino PS1 | Sinal PS1 | Direção (Console) | Conexão ESP32 (Porta 1) | Conexão ESP32 (Porta 2) | Observações Elétricas |
| :---: | :---: | :---: | :---: | :---: | :--- |
| **1** | **DATA** | Entrada | **GPIO 19** | **GPIO 22** | Linha serial de dados (Open-Drain, pull-up 3.5V no console) |
| **2** | **CMND** | Saída | **GPIO 32** | **GPIO 27** | Linha de comando serial do console |
| **3** | **7.6V** | Saída | *Não conectar* | *Não conectar* | Tensão dos motores de vibração originais |
| **4** | **GND** | Referência | **GND** | **GND** | Referência comum de terra (0V) |
| **5** | **3.5V** | Saída | **3.3V** | **3.3V** | Alimentação lógica (conectar no pino 3.3V do ESP32; **nunca no VIN**) |
| **6** | **ATT (SEL)** | Saída | **GPIO 34** | **GPIO 5** | Seleção de porta (nível baixo ativo) |
| **7** | **CLK** | Saída | **GPIO 33** | **GPIO 26** | Clock SPI serial (~250 kHz) |
| **9** | **ACK** | Entrada | **GPIO 21** | **GPIO 25** | Pulso de confirmação de recepção de byte |

### 2.2. LEDs Indicadores de Modo Analógico (Opcional)

| Sinal | Conexão ESP32 | Descrição |
| :--- | :---: | :--- |
| **LED Analógico Porta 1** | **GPIO 12** | Nível alto (3.3V) quando a Porta 1 está em modo Analógico |
| **LED Analógico Porta 2** | **GPIO 15** | Nível alto (3.3V) quando a Porta 2 está em modo Analógico |
| **LED Onboard (Status BT)**| **GPIO 2** | LED embutido da placa de desenvolvimento (status de conexão) |

---

## 3. Diretrizes de Alimentação

1. **Alimentação Direta pelos 3.5V do Console (Pino 5):**
   * Deve ser conectada diretamente ao pino **3.3V** do módulo ESP32.
   * **Não conecte a linha de 3.5V ao pino VIN/5V do ESP32**: o pino VIN alimenta um regulador linear LDO (ex.: AMS1117) cuja queda de tensão interna (*dropout*) exige no mínimo 4.5V a 5.0V para operar, o que causará desligamentos espontâneos (*brownout*) no microcontrolador.
2. **Alimentação Alternativa via Regulador Step-Down (Recomendado para estabilidade máxima):**
   * Utilize um mini conversor Buck DC-DC (ex.: MP1584EN) conectado ao **Pino 3 (7.6V)** da porta de controle.
   * Ajuste o regulador para **5.0V** estáveis e conecte a saída ao pino **VIN (5V)** do ESP32.

---

## 4. Instalação e Gravação do Firmware

### 4.1. Pacote Automatizado para Windows

1. Baixe o pacote **`BlueRetro_PS1_Custom_v0.2_Package.zip`** disponível na aba [Releases](https://github.com/Schickacka/BlueRetro/releases).
2. Extraia o conteúdo em uma pasta local.
3. Conecte o ESP32 ao computador via cabo USB.
4. Execute o arquivo **`flash_windows.bat`** e informe a porta serial identificada (exemplo: `COM6`).

### 4.2. Gravação Manual via Linha de Comando (esptool)

Caso prefira gravar manualmente via terminal:
```bash
python -m esptool --chip esp32 --port COMx -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 BlueRetro_hw1_playstation.bin
```

---

## 5. Compilação a Partir do Código-Fonte

O projeto utiliza o framework ESP-IDF v5.5 (ou o container oficial do BlueRetro):

```bash
# 1. Clonar o repositorio recursivamente com os submodulos (obrigatorio)
git clone --recursive https://github.com/Schickacka/BlueRetro.git
cd BlueRetro

# 2. Configurar o alvo para hardware 1 (PlayStation)
cp configs/hw1/playstation sdkconfig

# 3. Compilar
BR_HW=_hw1 BR_SYS=_playstation idf.py reconfigure build
```

---

## 6. Licença e Reconhecimentos

* **BlueRetro:** Copyright (c) 2019-2025, Jacques Gagnon. Distribuído sob a licença **Apache-2.0**.
* **Modificações para PlayStation 1 (Custom Edition):** Desenvolvido e mantido por **Schickacka**. Em conformidade com a seção 4(b) da licença Apache-2.0, todos os arquivos com alterações estruturais contêm notas e cabeçalhos de modificação.
