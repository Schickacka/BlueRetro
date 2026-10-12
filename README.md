# 🎮 BlueRetro — PlayStation 1 Custom Edition (v0.1)

<p align="center">
  <img src="static/PNGs/BRE_Logo_Color_Outline.png" width="450"/>
</p>

<p align="center">
  <b>Firmware customizado de alta performance para receptor interno/externo BlueRetro no PlayStation 1 (ESP32 DevKit).</b><br>
  <i>Desenvolvido por <b>Schickacka</b> com base no projeto pioneiro de Jacques Gagnon (<a href="https://github.com/darthcloud/BlueRetro">BlueRetro</a>).</i>
</p>

---

## 🚀 O que esta versão traz de novo (Changelog v0.1)

Esta edição foi desenvolvida sob medida para resolver limitações do BlueRetro clássico em instalações no PlayStation 1 (especialmente consoles PU-8, SCPH-3500 e posteriores), aprimorando a experiência com **DualShock 4** e **DualSense (PS5)**:

### 1. 💡 LED Onboard Inteligente (GPIO 2 no ESP32)
* **0 Controles conectados:**
  * Modo busca / pareamento (`inquiry` ativo): **Pulsando / Respirando suavemente**.
  * Sem busca: **Totalmente apagado**.
* **Apenas Controle 1 conectado:**
  * **Aceso contínuo** (100% aceso).
* **Controle 1 + outros controles (P1 + P2, etc.):**
  * Permanece aceso contínuo, emitindo piscadinhas rápidas **apagadas** (*winks* de 100ms) a cada 3 segundos indicando a quantidade de controles conectados.
* **Controle 1 desconectado, mas outro controle conectado (ex: apenas o P2):**
  * Permanece apagado e pisca **aceso** a cada 3 segundos o número de vezes correspondente ao maior controle conectado (ex: 2 piscadas para o P2).

### 2. 📳 Feedback Háptico na Conexão (Estilo 8BitDo)
* Assim que o controle Bluetooth conecta, ele vibra os motores em pulsos rápidos de **0,5 segundo** (500ms) com pausas de **0,3 segundo** indicando o número do jogador:
  * **Controle 1:** 1 toque ágil de 0.5s.
  * **Controle 2:** 2 toques ágeis de 0.5s (0.5s vibra ➔ 0.3s pausa ➔ 0.5s vibra).
  * **Controle 3 / 4:** 3 e 4 toques respectivamente.

### 3. 🌙 Modo Noturno com Brilho a 50% (Eye-Friendly)
* A barra de luz do DualShock 4 e DualSense foi recalibrada para 50% de intensidade, perfeita para jogar no escuro sem ofuscar a visão:
  * **Modo Analógico (SCPH-1200):** Vermelho suave (`R=128`).
  * **Modo Digital:** Branco/Cinza ambiente discreto (`RGB=16, 16, 16`).
  * **Respiração de Conexão:** Onda azul suave até 50% (`B=128`).

### 4. 🔄 Correção de Travamento de Analógico no RESET (Caso *Resident Evil 3*)
* Jogos como *Resident Evil 3* ativam o modo Analógico via software (`0x73`). Ao pressionar o botão físico **RESET** do PS1, o console reinicia a BIOS enquanto o barramento SPI silencia temporariamente.
* Nesta versão, implementamos um **timeout de inatividade de 1.5s**: se o console for resetado e o barramento SPI parar de se comunicar, o receptor reverte automaticamente para **Digital (`0x41`)**, apaga o LED analógico e volta o controle para o tom neutro.
* O botão **PS** agora é verificado continuamente via polling em segundo plano (<0,1 µs de CPU), permitindo alternar Digital/Analógico a qualquer momento sem travar.

### 5. 👥 Isolamento Total de Hotplug Multiplayer & Vibração no P2
* Corrigido o bug onde desconectar o Controle 2 deixava o Controle 1 inoperante na tela da BIOS.
* O Controle 2 agora possui suporte completo a vibração em jogos (*Tony Hawk's Pro Skater 2*, etc.) e suporte a controles paralelos/clones de PS4.

### 6. ⚡ Zero Input Lag
* O barramento SPI com o PlayStation roda em código *bare-metal* dedicado no **Core 1** do ESP32 com resposta em microssegundos (idêntico a um controle físico original). Todas as nossas rotinas adicionais rodam no Core 0 sem adicionar sequer 1 microssegundo de atraso.

---

## 🛠️ Esquema de Ligação (Pinout de Hardware)

### Conexão do ESP32 DevKit à Porta de Controles do PS1

```text
               [ 9   7   6 ]   [ 5   4   3 ]   [ 2   1 ]  (Porta do Console PS1)
               -----------------------------------------
```

| Pino PS1 | Sinal PS1 | Conexão ESP32 (Porta 1) | Conexão ESP32 (Porta 2) | Observações |
| :---: | :---: | :---: | :---: | :--- |
| **1** | **DATA** | **GPIO 19** | **GPIO 22** | Linha de dados para o PS1 (Requer pull-up de 1kΩ–10kΩ para 3.3V se externo) |
| **2** | **CMND** | **GPIO 23** | **GPIO 23** | Linha de comando compartilhada |
| **3** | **7.6V** | *Não conectar ao ESP32* | *Não conectar* | Tensão do motor de vibração original do console |
| **4** | **GND** | **GND** | **GND** | Terra comum |
| **5** | **3.5V** | **3.3V** *(ou VIN)* | **3.3V** *(ou VIN)* | Linha de alimentação lógica |
| **6** | **ATT (SEL)** | **GPIO 5** | **GPIO 26** | Seleção individual de porta |
| **7** | **CLK** | **GPIO 18** | **GPIO 18** | Clock SPI compartilhado |
| **9** | **ACK** | **GPIO 21** | **GPIO 27** | Confirmação de recebimento |

> 💡 **Dica de Instalação Interna:** Se estiver instalando internamente na placa-mãe (ex: PU-8), solde os fios diretamente nos pontos de teste (test points) ou no verso da placa do conector de controles.

---

## 📦 Como Instalar / Gravar o Firmware no ESP32

### Método 1: Pacote Rápido para Windows (Recomendado)

1. Baixe o arquivo **[`BlueRetro_PS1_Custom_v0.1_Package.zip`](https://github.com/Schickacka/BlueRetro/releases)** na aba **Releases**.
2. Extraia o arquivo zip em qualquer pasta do computador.
3. Conecte o ESP32 na porta USB do PC.
4. Dê dois cliques em **`flash_windows.bat`**.
5. Digite a sua porta COM (exemplo: `COM6`) e pressione Enter.
6. Pronto! O script gravará as partições e o firmware automaticamente com verificação.

---

### Método 2: Instalação Manual via Terminal (Qualquer SO)

Certifique-se de ter o Python e o `esptool` instalados:
```bash
pip install esptool
```

Execute o comando de gravação diretamente:
```bash
python -m esptool --chip esp32 --port COM6 -b 921600 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 40m 0x1000 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 BlueRetro_hw1_playstation.bin
```
*(Substitua `COM6` pela porta serial do seu ESP32 no Windows, ou `/dev/ttyUSB0` no Linux/macOS).*

---

## 🎮 Como Parear e Usar os Controles

### Pareando o Controle:
1. Ligue o PlayStation 1 com o receptor BlueRetro conectado.
2. O LED azul onboard do ESP32 começará a **respirar suavemente**, indicando que está aguardando conexão.
3. No seu controle:
   * **DualShock 4:** Segure os botões **PS + SHARE** juntos até a barra de luz piscar em rajadas duplas.
   * **DualSense (PS5):** Segure os botões **PS + CREATE** juntos até os LEDs piscarem rápido.
4. O controle conectará em poucos segundos:
   * Dará uma vibração tátil de **0,5 segundo**.
   * A barra de luz respirará em tom azul indicando o número do jogador e depois assumirá o modo de jogo.

### Alternando entre Modo Digital e Modo Analógico:
* Basta apertar o botão **PS** no controle a qualquer momento:
  * **Modo Analógico ATIVO:** A barra de luz acende em **Vermelho** (reproduzindo o LED físico do SCPH-1200) e os dois analógicos são ativados.
  * **Modo Digital (Analógico DESLIGADO):** A barra de luz acende em **Branco/Cinza suave** e os analógicos são desligados (ideal para jogos 2D clássicos como *Crash Bandicoot 1*, *Ridge Racer*, etc.).

---

## 📜 Licença e Agradecimentos

* **BlueRetro:** Copyright (c) 2019-2025 Jacques Gagnon (Licença Apache-2.0 / CERN-OHL-P-2.0).
* **Modificações & Customizações para PS1:** Desenvolvido e testado por **Schickacka**.
