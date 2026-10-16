# BlueRetro — PlayStation 1 Custom Edition (v0.4)

<p align="center">
  <img src="https://raw.githubusercontent.com/darthcloud/BlueRetro/master/docs/img/BlueRetro_logo.png" alt="BlueRetro Logo" width="320">
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

## 📖 Visão Geral

O **BlueRetro — PlayStation 1 Custom Edition** é um firmware de alta fidelidade desenvolvido especificamente para receptores Bluetooth baseados em **ESP32 (DevKit V1 / ESP-WROOM-32)** instalados interna ou externamente no console **Sony PlayStation 1** (compatível com todas as placas-mãe: PU-7, PU-8, PU-18, PU-20, PU-22, PU-23 e PSOne PM-41).

Baseado no projeto de engenharia reversa e emulação de controladores sem fio de Jacques Gagnon ([BlueRetro](https://github.com/darthcloud/BlueRetro)), esta edição personalizada foi projetada e exaustivamente calibrada por **Schickacka** para sanar limitações eletromecânicas e comportamentais do firmware genérico quando pareado com controles modernos (**DualShock 4**, **DualSense (PS5)**, **Switch Pro**, **Xbox Wireless**, **8BitDo**).

---

## 🎯 Principais Destaques e Engenharia Reversa da v0.4

### 1. Fidelidade Eletromecânica ao DualShock 1 (SCPH-1200) e Fim do Rumble Infinito (*Alien Resurrection*)
* **O Problema Histórico:** No jogo *Alien Resurrection*, quando a personagem Ellen Ripley atinge nível crítico de saúde, os motores do controle vibram em cadência sincrônica imitando um batimento cardíaco (*"tu-dum"*). No BlueRetro clássico, enquanto o motor leve vibrava, o motor pesado de baixa frequência entrava em rotação contínua ininterrupta, arruinando a experiência.
* **Causa Raiz Eletromecânica:** No controle físico original DualShock 1 (SCPH-1200), o motor esquerdo possui um contrapeso de ferro maciço acionado a 7.6V. Devido ao alto atrito estático e inércia do peso, comandos com valor PWM abaixo de **`0x40` (64 decimal)** não geram torque de partida suficiente para girar o motor físico, permanecendo completamente estático. O jogo *Alien Resurrection* envia comandos contínuos residuais de baixa amplitude (`< 0x40`) como ruído de fundo. Ao receber esses valores baixos, atuadores modernos de 3.7V giravam sem parar.
* **A Solução:** Implementada uma **Deadzone Eletromecânica de `0x40` (64)** no driver `ps.c`:
  $$\text{Power}_{\text{LF}} = \begin{cases} \text{raw}, & \text{se } \text{raw} \ge 0x40 \\ 0, & \text{se } \text{raw} < 0x40 \end{cases}$$
* **Resultado:** O contrapeso pesado permanece em repouso absoluto durante os intervalos entre as batidas cardíacas e entra em ação apenas nos pulsos de impacto firme. O motor leve (direito) opera com saturação plena (`0xFF`), reproduzindo o batimento cardíaco com fidelidade idêntica à do hardware original.

---

### 2. Vibração Contínua em Alta Frequência (20 ms / 50 Hz) (*Resident Evil 3: Nemesis*)
* **O Problema:** Durante disparos sustentados de armas automáticas (como a metralhadora de Jill Valentine em *Resident Evil 3*), a vibração apresentava solavancos, microcortes e interrupções aleatórias, não mantendo um fluxo contínuo.
* **Causa Raiz nos Controles Bluetooth:** Todo controle sem fio moderno (**DualShock 4**, **DualSense**, etc.) possui em sua MCU interna um temporizador de segurança (*watchdog*) de **~200 ms a 250 ms**. Se os motores estão acionados e nenhum novo pacote Bluetooth chega nesse intervalo, o controle desliga os motores para evitar superaquecimento e esgotamento da bateria caso a conexão caia. Como o jogo envia valores idênticos quadro a quadro durante a rajada de tiros, o BlueRetro original entrava em modo de repouso por 300 ms (`BT_FB_TASK_DELAY_CNT = 30`), disparando o watchdog interno do controle a cada ~200 ms.
* **A Solução:** A tarefa de feedback Bluetooth (`bt_fb_task`) foi reprojetada com o flag atômico **`BT_DEV_RUMBLE_ON`**. Enquanto qualquer dispositivo conectado estiver com vibração ativa, o BlueRetro retransmite o relatório háptico a cada **20 ms (50 Hz)** — exatamente na cadência síncrona dos quadros de vídeo do PlayStation 1.
* **Resultado:** Disparos contínuos de metralhadora agora mantêm uma vibração contínua, densa, homogênea e sem nenhuma microinterrupção enquanto o gatilho permanecer pressionado.

---

### 3. Corte Instantâneo de Vibração em Pausa e Menus (`0x43`)
* Ao pressionar Start para pausar o jogo ou entrar em inventários/telas de configuração, o console transmite o comando `0x43`.
* O driver agora intercepta essa transição imediatamente e despacha um pacote de parada forçada com `data_len = 0`, zerando os motores sem atrasos ou resíduos de rotação.

---

### 4. Tolerância Estendida a Cutscenes em FMV e Resets (3,0 Segundos)
* Jogos como *Resident Evil 3* interrompem temporariamente a varredura do barramento SPI durante reprodução de vídeos em tela cheia (FMV) e carregamentos de salas no CD-ROM.
* O limiar de reset ocioso do barramento (`PS_IDLE_RESET_US`) foi ajustado para **3,0 segundos**, impedindo que cutscenes desativem o modo analógico acidentalmente, mas garantindo que o controle retorne a Digital caso o console seja reiniciado pelo botão físico de Reset.

---

### 5. Sincronização Instantânea de Modo Analógico e LED
* **Sincronização na Conexão:** Ao ligar o controle no meio de um jogo que já esteja em modo Analógico (`0x73`), o controle entra imediatamente em Analógico e acende a barra de luz na cor correta sem exigir reinício do console.
* **Modo Noturno (50% de Brilho):** Para não ofuscar o jogador em ambientes escuros:
  * **Modo Analógico Ativo:** Barra de luz em **Vermelho** (homenagem à clássica lâmpada LED do DualShock SCPH-1200) com 50% de luminosidade (`#800000`).
  * **Modo Digital:** Barra de luz em **Cinza Suave / Branco Quente** a 50% (`#101010`).

---

### 6. Arquitetura Dual-Core e Sincronização Atômica
* O ESP32 opera com dois núcleos de processamento: **Core 1** dedicado ao driver baremetal SPI em tempo real (`ps_spi.c`) e **Core 0** dedicado à pilha de rádio Bluetooth (`bt_host.c`).
* Todas as variáveis compartilhadas de estado (`dev_id`, `pend_dev_id`, `rumble_r_val`, `rumble_l_val`, `analog_btn`, `last_spi_time`) utilizam qualificadores `volatile`, operações atômicas da API Zephyr e barreiras de memória de hardware `__sync_synchronize()`, garantindo consistência total livre de condições de corrida (*race conditions*).

---

## 🔌 Esquema Elétrico e Pinagem (Hardware Pinout)

O driver baremetal utiliza temporizadores e interrupções GPIO de alta velocidade. Para instalações com duas portas simultâneas (Porta 1 e Porta 2), cada porta possui pinagem dedicada:

```text
               [ 9   7   6 ]   [ 5   4   3 ]   [ 2   1 ]  (Porta Frontal do PS1)
               -----------------------------------------
```

| Pino PS1 | Linha de Sinal | Direção (Console) | Conexão ESP32 (Porta 1) | Conexão ESP32 (Porta 2) | Observações Elétricas |
| :---: | :---: | :---: | :---: | :---: | :--- |
| **1** | **DATA** | Entrada | **GPIO 19** | **GPIO 22** | Linha serial de dados (Open-Drain, pull-up 3.5V no console) |
| **2** | **CMND** | Saída | **GPIO 32** | **GPIO 27** | Linha de comando serial emitida pelo console |
| **3** | **7.6V** | Saída | *Não conectar* | *Não conectar* | Tensão dos motores originais do DualShock |
| **4** | **GND** | Referência | **GND** | **GND** | Referência de terra comum (0V) |
| **5** | **3.5V** | Saída | **3.3V** | **3.3V** | Alimentação lógica (ligar no pino **3.3V**; nunca no VIN) |
| **6** | **ATT (SEL)** | Saída | **GPIO 34** | **GPIO 5** | Seleção de porta (nível baixo ativo) |
| **7** | **CLK** | Saída | **GPIO 33** | **GPIO 26** | Sinal de clock serial SPI (~250 kHz) |
| **9** | **ACK** | Entrada | **GPIO 21** | **GPIO 25** | Pulso de confirmação de recepção de byte |

### LEDs Opcionais de Modo Analógico

| Sinal | Conexão ESP32 | Descrição |
| :--- | :---: | :--- |
| **LED Analógico Porta 1** | **GPIO 12** | Nível alto (3.3V) quando a Porta 1 opera em Modo Analógico |
| **LED Analógico Porta 2** | **GPIO 15** | Nível alto (3.3V) quando a Porta 2 opera em Modo Analógico |
| **LED Onboard (Status BT)**| **GPIO 2** | LED azul embutido da placa DevKit (pisca ao aguardar pareamento) |

---

## ⚡ Recomendações Críticas de Alimentação

1. **Alimentação Direta pela Linha de 3.5V (Pino 5):**
   * Deve ser conectada diretamente ao pino **`3.3V`** do ESP32.
   * ⚠️ **NUNCA ligue a linha de 3.5V ao pino `VIN` / `5V` do ESP32**: O pino VIN passa pelo regulador linear LDO onboard (ex.: AMS1117-3.3), cuja queda de tensão (*dropout*) exige pelo menos 4.5V a 5.0V na entrada. Ligar 3.5V no VIN fará o microcontrolador sofrer reinicializações espontâneas por subtensão (*brownout reset*).
2. **Alimentação via Regulador Step-Down (Ideal para Máxima Estabilidade):**
   * Conecte um mini módulo Buck DC-DC (ex.: MP1584EN) no **Pino 3 (7.6V)** do conector do console.
   * Ajuste o potenciômetro para fornecer **5.0V regulados** e conecte a saída ao pino **`VIN` (5V)** do ESP32.

---

## 🚀 Como Atualizar / Gravar o Firmware

### Opção 1: Atualização Sem Fio via Bluetooth (OTA - Recomendado)
Se o BlueRetro já estiver instalado dentro do PlayStation 1, você não precisa abrir o videogame nem plugar cabos:

1. Baixe o pacote **[`BlueRetro_PS1_Custom_v0.4_Package.zip`](https://github.com/Schickacka/BlueRetro/releases)** na aba Releases.
2. Ligue o PlayStation 1 com os controles desligados.
3. Dê dois cliques no arquivo **`flash_bluetooth_ota.bat`**.
4. O script localiza o BlueRetro via Bluetooth LE e grava o firmware em aproximadamente 50 segundos. O BlueRetro reinicia automaticamente já atualizado.

*Alternativa via Navegador:* Acesse **[blueretro.io/ota.html](https://blueretro.io/ota.html)** no Chrome ou Edge, clique em **Connect**, selecione seu receptor e envie o arquivo `BlueRetro_hw1_playstation.bin`.

### Opção 2: Gravação via Cabo USB (Windows)
1. Conecte o ESP32 ao computador via cabo USB.
2. Extraia o pacote **`BlueRetro_PS1_Custom_v0.4_Package.zip`**.
3. Execute o arquivo **`flash_windows.bat`** e informe a porta serial (exemplo: `COM3`, `COM4`, `COM6`).

### Opção 3: Linha de Comando Manual (esptool)
```bash
python -m esptool --chip esp32 --port COMx -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 BlueRetro_hw1_playstation.bin
```

---

## 🎮 Matriz de Jogos e Controles Validados

| Jogo | Teste Realizado | Resultado com a v0.4 |
| :--- | :--- | :---: |
| **Alien Resurrection** | Batimento cardíaco com baixa saúde (*Ellen Ripley*) | ✅ **Perfeito** (Motor forte silencioso no repouso; motor fraco vibrando forte no *"tu-dum"*) |
| **Resident Evil 3: Nemesis** | Disparo sustentado de Metralhadora / Gatling Gun | ✅ **Perfeito** (Vibração contínua a 50 Hz sem cortes ou solavancos) |
| **Resident Evil 3: Nemesis** | Reprodução de FMVs e transições de tela | ✅ **Perfeito** (Sem perda de estado analógico durante pausas de SPI) |
| **Gran Turismo 2** | Aceleração contínua e zebras de pista | ✅ **Perfeito** (Resposta gradual precisa em ambas as portas) |
| **Silent Hill** | Rádio chiando e batimentos de monstros | ✅ **Perfeito** (Sincronia precisa de pulso háptico) |
| **Metal Gear Solid** | Sessão de tortura e disparos de FAMAS | ✅ **Perfeito** (Alta intensidade contínua sem desarmes) |
| **Tekken 3** | Golpes fortes e especiais de impacto | ✅ **Perfeito** (Impacto agudo instantâneo) |

### Controles Sem Fio Homologados
* **Sony DualShock 4** (todas as revisões: CUH-ZCT1 e CUH-ZCT2).
* **Sony DualSense (PS5)** (atuadores hápticos mapeados para rotação de baixa e alta frequência).
* **Nintendo Switch Pro Controller** e Joy-Cons.
* **Xbox Wireless Controller** (Modelos 1708, Series X/S 1914).
* **8BitDo** (Pro 2, Ultimate Bluetooth, SN30 Pro, M30).

---

## 🛠️ Compilação a Partir do Código-Fonte

Este repositório possui integração contínua (CI) com **GitHub Actions** que gera os binários automaticamente a cada commit. Caso queira compilar localmente com o **ESP-IDF v5.5**:

```bash
# Clonar com todos os submódulos (obrigatório)
git clone --recursive https://github.com/Schickacka/BlueRetro.git
cd BlueRetro

# Carregar configuração do hardware PlayStation
cp configs/hw1/playstation sdkconfig

# Compilar
BR_HW=_hw1 BR_SYS=_playstation idf.py reconfigure build
```

---

## 📜 Licença e Créditos

* **BlueRetro:** Copyright © 2019–2025 Jacques Gagnon. Distribuído sob a licença **Apache License 2.0**.
* **PlayStation 1 Custom Edition:** Desenvolvido, testado e mantido por **Pedro Afonso (Schickacka)**.
* Todas as alterações de código estão devidamente documentadas nos cabeçalhos e arquivos modificados em conformidade com os termos da licença Apache 2.0.
