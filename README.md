# BlueRetro — PlayStation 1 Custom Edition (v0.4)

Firmware customizado de alta performance para receptor Bluetooth BlueRetro voltado ao PlayStation 1 (ESP32 DevKit). Desenvolvido por **Schickacka**, baseado no projeto de engenharia reversa e emulação sem fio de Jacques Gagnon ([BlueRetro](https://github.com/darthcloud/BlueRetro)).

---

## 1. Visão Geral das Modificações da Versão 0.4

Esta edição foi projetada para resolver limitações do BlueRetro clássico em instalações no PlayStation 1 (consoles PU-7, PU-8, PU-18, PU-20, PU-22, PU-23 e PSOne PM-41), com foco em controles **DualShock 4** e **DualSense (PS5)**:

* **Fidelidade Eletromecânica ao DualShock 1 (SCPH-1200) e Correção do Rumble Infinito (*Alien Resurrection*):**
  * **Deadzone Eletromecânica do Motor Pesado (`0x40` / 64):** No controle físico DualShock 1 original, o motor esquerdo (contrapeso pesado) exige no mínimo **`0x40` (64)** em comando PWM para superar a inércia estática e atrito do peso excêntrico de ferro sob 7.6V. Valores entre `0x01` e `0x3F` não possuem torque suficiente para girar o motor físico, permanecendo completamente imóveis. Jogos como *Alien Resurrection* emitem valores residuais baixos (`< 0x40`) como linha de base durante o gameplay. No BlueRetro clássico/v0.3, esses valores baixos eram repassados para atuadores modernos de 3.7V, fazendo o contrapeso girar indefinidamente. Na **v0.4**, comandos abaixo de `0x40` são tratados fielmente como repouso (`0x00`), permitindo que a simulação de batimento cardíaco ("tu-dum") em baixa saúde pulse com máxima força e cesse com silêncio cirúrgico entre as batidas.
  * **Ativação Completa do Motor de Alta Frequência (Direito / Fraco):** O driver trata qualquer valor não nulo como pulso ativo do motor direito (`hf_pwr = 0xFF`), garantindo o impacto agudo síncrono.
  * **Vibração Contínua Sustentada (ex.: Metralhadora em *Resident Evil 3*):** Fluxo háptico contínuo garantido sem engasgos ou cortes arbitrários enquanto o gatilho permanecer acionado.
  * **Corte Instantâneo de Vibração ao Entrar em Modo de Configuração / Pause (`0x43`):** Interrompe imediatamente qualquer vibração ativa ao pausar ou acessar menus.
  * **Corte de Vibração no Reset do Console:** Em caso de reset do PS1, qualquer comando de vibração pendente é limpo imediatamente.
* **Sincronização Atômica entre Núcleos (Core 1 ↔ Core 0):**
  * Campos de estado (`dev_id`, `pend_dev_id`, `rumble_*`, `analog_btn`, `last_spi_time`) declarados como `volatile` com barreiras `__sync_synchronize()`.
* **Tratamento Robusto de Comandos SPI e Fallback Neutro:**
  * Comandos desconhecidos retornam nível neutro High-Z (`0xFF`) sem emitir ACK espúrio.
* **Detecção Inteligente de Reset do Console (Caso *Resident Evil 3*):**
  * Limiar estendido para 3,0 segundos (`PS_IDLE_RESET_US`), evitando falsos disparos durante cutscenes em FMV e carregamentos de CD-ROM.
* **Sincronização Imediata de LED na Conexão:**
  * O controle já acende diretamente em Vermelho se conectado com o jogo operando em analógico (`0x73`).
* **Padrão de LED Onboard Dinâmico (GPIO 2):**
  * Ciclos dimensionados para suportar topologias multitap completas com cache de estado nos registradores GPIO.
* **Feedback Háptico na Conexão:**
  * Pulso suave ao parear identificando a porta do jogador.
* **Modo Noturno com Brilho a 50%:**
  * Barra de luz balanceada (Vermelho a 50% para Analógico, Cinza ambiente a 50% para Digital).

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

### 4.1. Atualização Sem Fio via Bluetooth (OTA - Recomendado)

Se o seu BlueRetro já estiver instalado no PlayStation 1, você pode atualizar sem remover o console nem conectar cabos:

* **Pelo Script Incluso no Pacote (Windows):**
  1. Ligue o PlayStation 1 com os controles desligados (o LED onboard respirará indicando espera).
  2. Dê dois cliques em **`flash_bluetooth_ota.bat`** dentro do pacote [`BlueRetro_PS1_Custom_v0.4_Package.zip`](https://github.com/Schickacka/BlueRetro/releases).
  3. O script localiza o dispositivo via Bluetooth LE e grava o firmware em ~50 segundos com reboot automático.

* **Pelo Navegador (Chrome / Edge):**
  1. Acesse **[blueretro.io/ota.html](https://blueretro.io/ota.html)**.
  2. Clique em **Connect**, selecione `BlueRetro_PSX` e escolha o arquivo `BlueRetro_hw1_playstation.bin`.
  3. Clique em **Update** e aguarde a barra atingir 100%.

### 4.2. Pacote Automatizado para Windows (via Cabo USB)

1. Baixe o pacote **`BlueRetro_PS1_Custom_v0.4_Package.zip`** na aba [Releases](https://github.com/Schickacka/BlueRetro/releases).
2. Extraia o conteúdo e conecte o ESP32 ao computador via cabo USB.
3. Execute o arquivo **`flash_windows.bat`** e informe a porta serial (exemplo: `COM6`).

### 4.3. Gravação Manual via Linha de Comando (esptool)

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
