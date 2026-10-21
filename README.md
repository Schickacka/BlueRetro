# BlueRetro — PlayStation 1 Custom Edition (v0.6.2)

<p align="center">
  <img src="https://raw.githubusercontent.com/darthcloud/BlueRetro/master/docs/img/BlueRetro_logo.png" alt="BlueRetro Logo" width="300">
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

## Novidades e Correções da Versão 0.6.2

* **Rumble Ativo em Modo Digital no PS1 (`0x41`):** Correção do gate de vibração em `ps_spi.c` para filtrar exclusivamente por tipo de dispositivo (`port->dev_type[id] != DEV_PS2_DS2`) em vez de identificador de modo (`dev_id`). Isso preserva o comportamento autêntico do fork de permitir vibração mesmo quando o PlayStation 1 ainda enxerga o controle como digital (`0x41`), antes da alternância para analógico (`0x73`) pelo jogo ou pelo botão PS/Analog.
* **Fidelidade Cromática dos LEDs de Jogador no DualShock 4:** Os LEDs de jogador do DualShock 4 passam a utilizar `ps1_led_color(ps1_is_analog_for(bt_data))` e `ps1_battery_color()`, respeitando as cores configuradas no Kconfig e atenuando adequadamente em Modo Noturno, idêntico ao comportamento do DualSense.
* **Prevenção de Sobrescrita do Rumble pelo Pulso de Pareamento:** Ao término do pulso de conexão de 150 ms, os motores só são restaurados para o estado de repouso se nenhum comando de vibração tiver sido enviado pelo jogo durante a janela de pareamento (`HF=0xFF, LF=0x00`).
* **Suporte a Acessos Atravessando Blocos no Memory Card Virtual:** Implementada leitura e escrita contínua com quebra de blocos de 4 KB em `mc_read()` e `mc_write()`, bounds checking estrito e retorno seguro de bloco zerado em `mc_get_ptr()`.

---

## Novidades e Correções da Versão 0.6.1

* **Correção do Relatório Bluetooth 0x31 do DualSense (PS5):** Implementação exata da especificação oficial do relatório Bluetooth do DualSense (`struct bt_hidp_ps5_set_conf` de 78 bytes com `report ID 0x31`): contador incremental de sequência de saída (`seq_tag`), tag obrigatória `0x10`, ajuste do padding reservado para 23 bytes (mantendo alinhamento preciso do CRC32 nos offsets 73..76).
* **Proteção de Vibração em Controles Digitais:** Bloqueio de comandos de vibração (0x42) quando a porta estiver identificada como controle digital padrão (`0x41`), mantendo a integridade do protocolo SPI do PS1 e preservando a liberação de vibração sem exigência da flag `acc_mode` para controles analógicos/DualShock (`0x73`, `0x79`, `0xF3`).
* **Trava Analógica no Reset do Console (`analog_lock`):** Prevenção de comutação indesejada para modo analógico caso o botão PS / analógico esteja pressionado durante o Reset do console; o controle aguarda a liberação física do botão antes de permitir nova alternância.
* **Prevenção de Vazamento de Temporizadores (`esp_timer`):** Cancelamento seguro e desalocação de temporizadores pendentes de inicialização em desconexões rápidas (`host.c`, `ps3.c`, `sw.c`, `ps.c`), eliminando callbacks executando sobre estruturas recicladas.
* **Preservação das Configurações de Acessórios:** Remoção do laço de reinicialização forçada em `ps_spi.c` que sobrepunha as opções de acessórios configuradas pelo usuário (`ACC_BOTH`, `ACC_NONE`, etc.).
* **Correção Cromática do Modo Noturno e Indicação de Bateria:** Correção da ordenação de bytes no cálculo de cor do modo noturno (`0x00BBGGRR`), preservando matriz cromática para qualquer cor selecionada, e pulso âmbar suave a cada 30 segundos em níveis críticos de bateria (<20%).
* **Robustez e Integridade do Memory Card Virtual:** Correção do tamanho de blocos para `MC_BUFFER_BLOCK_SIZE` (4096 bytes), inicialização limpa com `0xFF` (padrão de cartão virgem PlayStation), checagem estrita de limites (*bounds checking*), tratamento de re-tentativa para blocos pendentes de gravação e alocação dinâmica protegida em `mc_init_mem`.

* **Suporte Nativo aos LEDs de Jogador do DualSense (PS5):** Mapeamento nativo dos LEDs indicadores de jogador do DualSense para identificação da porta do console (Portas 1 a 4). A barra de luz RGB do DualSense segue a identidade visual do PlayStation 1: cinza suave (`#101010`) em modo Digital e vermelho clássico SCPH-1200 (`#6D0000`) em modo Analógico. O DualSense não pisca em azul na conexão (a porta já é indicada pelos LEDs dedicados), mas mantém o pulso tátil de conexão de 150 ms.
* **Correção Estrutural do Relatório HID do DualSense (77 Bytes):** Reestruturação do pacote HID 0x02 para 77 bytes com flags calibradas (`conf1 = 0x14`, `valid_flag2 = 0x02`, `lightbar_setup = 0x02`) e remoção do campo `use_accurate_rumble` que causava deslocamento de offsets no payload, restabelecendo a operação plena de vibração e iluminação.
* **Isolamento de Buffer HID (`bt_hid_pkt_tmp`):** Separação completa entre o buffer de pacotes HID (`bt_hid_pkt_tmp`) e o buffer de pacotes HCI/ATT/SDP (`bt_hci_pkt_tmp`), eliminando condições de corrida na geração de relatórios de feedback e prevenindo corrupção de CRC32 nos controles DualShock 4 e DualSense.
* **Eliminação de Temporizadores Efêmeros (`esp_timer`):** Substituição de temporizadores auto-destrutivos de blink e pulso háptico por máquinas de estado acionadas periodicamente por `bt_hid_ps_tick()` a cada 10 ms em `bt_fb_task`. Desconexões e reconexões rápidas não geram mais vazamentos de memória (*use-after-free*) ou travamentos do subsistema Bluetooth.
* **Sincronização Precisa de Inatividade Entre Núcleos:** Centralização da leitura e comparação de contadores de ciclo no Core 0 (`ps_spi_poll()`). A interrupção bare-metal do Core 1 (`packet_end()`) agora executa estritamente dois stores de 32 bits (incremento atômico de `pkt_cnt` e sinalização de `spi_active`), eliminando chamadas desnecessárias e reduzindo a latência da ISR ao mínimo teórico.
* **Correção de Mapeamento em Multitap e Reset de Modo:** Atualização de `ps_spi_poll()` e `ps_spi_is_analog()` para utilizar o índice real `mt_first_port`, corrigindo o mapeamento dos jogadores 3 e 4. O tratamento de reset do console foi estendido para restaurar também os identificadores `0x79` (analógico estendido) e `0xF3` (modo de configuração).
* **Preservação de Rumble em Controles Switch / 8BitDo:** O marcador de tipo de pacote `output[127]` agora é gravado consistentemente em todos os despachos de feedback, impedindo que o chaveamento de LEDs interrompa os efeitos de vibração em controles 8BitDo e Switch Pro.
* **Otimização de Memória PS1-Only (`BR_PS1_ONLY`):** Opção de build condicional que remove drivers de barramento de outros consoles (N64, GameCube, Saturn, SNES, etc.), liberando **23,6 KB de IRAM** e **10,9 KB de DRAM**.
* **Desativação Padrão do Memory Card Virtual (`CONFIG_BR_PS1_MEMORY_CARD=n`):** O Memory Card Virtual fica desativado por padrão, liberando **128 KB de heap DMA** para a pilha Bluetooth e conexões simultâneas de controles. Varredura periódica de integridade em `bt_fb_task` implementada para quando o recurso for habilitado opcionalmente via Kconfig.
* **Menu de Customização Kconfig (`PS1 Custom Edition`):** Opções configuráveis no menu do projeto para habilitar/desabilitar blink de porta, pulso háptico, modo noturno, indicação de bateria e instrumentação de latência para análise com osciloscópio.

---

## Novidades e Correções da Versão 0.5.1

* **Estabilidade do Modo Analógico na Porta 2:** Correção do algoritmo de detecção de Reset do console em `ps_spi_poll()`. Anteriormente, a verificação de inatividade (`PS_IDLE_RESET_US = 3.0s`) avaliava as portas de forma isolada; como grande parte dos jogos de PS1 não envia quadros contínuos para a Porta 2 durante o modo single-player ou telas de menu, a Porta 2 sofria timeout prematuro e desativava o modo analógico espontaneamente a cada 3 segundos. O monitoramento agora valida a atividade global do barramento: se a Porta 1 estiver comunicando ativamente com o console (jogo em execução), a Porta 2 permanece continuamente em modo analógico com o LED vermelho aceso. A reversão para digital ocorre estritamente quando todo o barramento do console silenciar de forma simultânea (indicando acionamento físico do botão Reset no videogame).
* **Vibração de Conexão no Controle 2 e Demais Portas:** Remoção da trava indevida que condicionava a vibração de pareamento Bluetooth à flag `acc_mode & ACC_RUMBLE`. O pulso de confirmação háptica de pareamento (150 ms no atuador leve, sem acionar o motor pesado) agora dispara normalmente para qualquer controle que conectar (Controle 1, Controle 2, etc.), independente do estado de configuração de acessórios na partição NVS.
* **Atenuação Visual do LED Analógico (Redução de 15%):** Ajuste fino da intensidade da barra de luz em modo analógico (SCPH-1200) de `128` (`0x80`) para `109` (`0x6D`) nos controladores DualShock 4 e DualSense, mantendo o visual clássico sem ofuscar o jogador em ambientes escuros.
* **Preservação Eletromecânica Fiel:** Manutenção do comportamento háptico autêntico do DualShock 1 no motor de baixa frequência (`0x40` deadzone) e no motor leve (`0xFF` em saturação máxima), garantindo que apenas os atuadores solicitados pelo jogo vibrem, sem acionamento conjunto indevido do motor pesado.

---

## Melhorias Arquiteturais e Correções Técnicas (v0.5)

* **Eliminação de Condições de Corrida na Fila de Feedback (`queue_bss`):** A fila `queue_bss` (liblfds711) opera como buffer circular lock-free de produtor único e consumidor único. Para garantir integridade quando múltiplos contextos concorrentes em núcleos distintos (ISR bare-metal no Core 1 e tarefas FreeRTOS no Core 0) emitem eventos de feedback, a rotina `adapter_q_fb()` foi protegida por spinlock atômico (`fb_queue_lock`), serializando as inserções e eliminando o risco de corrupção de ponteiros.
* **Leitura de Ciclos de Hardware (CCOUNT) no Driver Baremetal:** Substituição da chamada `esp_timer_get_time()` dentro da interrupção bare-metal do Core 1 pelo registrador de ciclos de instrução do processador (`xthal_get_ccount()`). A leitura e o armazenamento do tempo em 32 bits eliminam a sobrecarga de chamadas ao sistema operacional em contexto de interrupção e impedem a divisão de palavras de 64 bits (*word tearing*) em barramento Xtensa de 32 bits.
* **Isolamento de Estado do Botão Analógico:** A alternância de modo via botão PS/Home em `ps_analog_btn_hdlr()` foi protegida por trava atômica (`ps_analog_busy`), prevenindo colisões de estado entre o laço de varredura `ps_spi_poll()` e a ISR `packet_end()`.
* **Restauração do Suporte a Memory Card Virtual (`ACC_MEM`):** Remoção da sobrescrita estática que forçava `acc_mode = ACC_RUMBLE` em `ps_spi_init()`, restabelecendo o respeito às opções configuradas pelo usuário na memória não volátil e permitindo o funcionamento pleno do Memory Card Virtual.
* **Serialização de Transmissão Bluetooth (`bt_hci_pkt_tmp`):** Implementação de mutex recursivo (`bt_tx_mutex`) para acesso ao buffer global `bt_hci_pkt_tmp`. Evita que rotinas de temporização e a tarefa periódica `bt_fb_task` colidam durante a montagem do cabeçalho L2CAP/HCI ou cálculo de CRC32, eliminando descartes de pacotes por checksum corrompido nos controles DualShock 4 e DualSense.
* **Proteção Atômica contra Deleção Dupla de Temporizadores:** Uso de troca atômica com barreira de memória (`__atomic_exchange_n`) em `ps4_blink_stop()`, `ps4_blink_timer_cb()` e `ps_conn_rumble_timer_cb()`, garantindo que temporizadores FreeRTOS sejam desarmados e destruídos de forma estritamente única, prevenindo falhas de *use-after-free* em desconexões simultâneas.
* **Mapeamento Preciso de Status em Multitap (Jogadores 3 e 4):** A função `ps_spi_is_analog()` foi reescrita para considerar a topologia de portas e slots configurada (Multitap na Porta 1, Porta 2 ou Dual), permitindo que controles conectados aos slots subsequentes exibam corretamente as cores do LED de modo.
* **Preservação de Rumble em Controles 8BitDo (Modo Switch):** O registrador interno de tipo de pacote (`output[127]`) agora é preservado durante eventos de LED de status, e o despachante de comandos verifica ativamente a flag `BT_DEV_RUMBLE_ON`, impedindo que relatórios de iluminação façam o controle 8BitDo chavear indevidamente para `SET_CONF` e cessar a vibração.
* **Prevenção de Saturação no Canal de Controle para Clones de DS4:** Remoção de pacotes `SET_REPORT` redundantes no canal de controle Bluetooth em transmissões periódicas a 50 Hz, mantendo o tráfego estritamente no canal de interrupção e prevenindo bloqueios por confirmação pendente (*handshake*).

---

## Arquitetura e Configurações de Hardware

### 1. Deadzone Eletromecânica do Atuador de Baixa Frequência (0x40 / 64)
* **Comportamento do Hardware Original:** No controle físico DualShock 1 (SCPH-1200), o motor esquerdo (baixa frequência) é composto por um contrapeso excêntrico de ferro maciço alimentado pela linha de 7.6V do console. Devido ao atrito estático e à inércia mecânica desse conjunto, comandos com sinal PWM inferior a `0x40` (64 em decimal) não produzem torque de partida suficiente para girar o motor físico, permanecendo completamente imóvel no hardware com fio original.
* **Compatibilidade com Controles Modernos:** Jogos de PlayStation 1 frequentemente emitem valores residuais contínuos de baixa amplitude (`< 0x40`) nos registradores de feedback como ruído de fundo ou nível de linha de base. Em controladores sem fio modernos equipados com atuadores de 3.7V (que possuem limiar de partida muito baixo), esses valores residuais faziam o contrapeso girar de forma contínua em momentos nos quais o controle com fio original permanecia em repouso absoluto.
* **Calibração Implementada:** Comandos de rotação do motor pesado com valores inferiores a `0x40` são tratados estritamente como repouso (`0x00`), acionando o motor apenas quando a intensidade exigida for igual ou superior a 64:
  $$\text{Power}_{\text{LF}} = \begin{cases} \text{raw}, & \text{se } \text{raw} \ge 0x40 \\ 0, & \text{se } \text{raw} < 0x40 \end{cases}$$
  Isso restaura a resposta tátil original do SCPH-1200 em toda a biblioteca do sistema, permitindo que efeitos modulados em pulso (como batimentos cardíacos, vibrações de advertência e passos) vibrem com impacto firme e cessem com silêncio cirúrgico entre as pulsações.
* **Ativação do Motor de Alta Frequência:** O motor direito (leve/alta frequência) é tratado como sinal binário digital, no qual qualquer byte não nulo aciona saturação máxima (`0xFF`), garantindo resposta tátil nítida e imediata.

### 2. Transmissão Contínua de Feedback em Alta Frequência (20 ms / 50 Hz)
* **Temporizador Watchdog dos Controles Sem Fio:** Os controladores Bluetooth modernos (DualShock 4, DualSense, Xbox Wireless, Switch Pro) possuem um temporizador interno de segurança (*watchdog*) em seus firmwares de aproximadamente 200 ms a 250 ms. Se os motores estiverem ativos e nenhum novo relatório HID Bluetooth for recebido dentro dessa janela, o controle atenua ou desliga a alimentação dos atuadores por proteção térmica e conservação de bateria.
* **Sustentação de Eventos Hápticos Prolongados:** Em situações de vibração contínua e constante (como disparos sustentados de armas automáticas, aceleração contínua em zebras de pista ou explosões de longa duração), o console mantém os mesmos valores nos registradores de comando quadro a quadro. Na ausência de variação nos dados, a rotina de feedback pausava a transmissão por 300 ms, gerando solavancos e microcortes cíclicos causados pelo desarmamento do watchdog interno do controle.
* **Calibração Implementada:** A rotina de transmissão Bluetooth (`bt_fb_task`) monitora atomicamente o estado ativo de vibração (`BT_DEV_RUMBLE_ON`). Enquanto qualquer controle conectado estiver vibrando, os pacotes de feedback são retransmitidos a cada **20 ms (50 Hz)** — taxa rigorosamente síncrona com os quadros de vídeo do hardware do PlayStation 1. Isso mantém o watchdog do controle continuamente alimentado, garantindo uma resposta de vibração uniforme, sólida e sem interrupções durante toda a sustentação do comando.

### 3. Interrupção Imediata de Vibração em Pausa e Menus (Comando 0x43)
* Quando o jogo entra em pausa ou acessa telas de configuração/inventário, o console chaveia a comunicação serial para o comando `0x43`. O driver intercepta imediatamente essa transição e emite um pacote de parada forçada com comprimento zero (`data_len = 0`), desligando os motores de forma instantânea sem resíduos de rotação.

### 4. Limiar Estendido de Inatividade do Barramento SPI (3,0 Segundos)
* Durante transições de tela, telas de carregamento do CD-ROM ou reprodução de vídeos em tela cheia (FMV), determinados jogos suspendem temporariamente a comunicação com as portas de controle por mais de um segundo. O limiar de inatividade para reinicialização (`PS_IDLE_RESET_US`) foi estendido para **3,0 segundos**, prevenindo que pausas de leitura do disco desarmem o modo analógico, enquanto preserva a capacidade de detectar com precisão quando o console foi reiniciado pelo botão físico de Reset.

### 5. Sincronização Imediata de Modo Analógico e Iluminação Balanceada
* **Sincronização na Conexão:** Ao ligar o controle no meio de uma partida na qual o console já esteja operando em modo Analógico (`0x73`), o controle entra diretamente em Analógico e acende a barra de luz na cor correspondente, sem exigir reinicialização do console ou alternância manual.
* **Iluminação Noturna a 50%:** A barra de luz RGB opera com luminosidade equilibrada para evitar ofuscamento em ambientes com baixa iluminação:
  * **Modo Analógico Ativo:** Vermelho clássico do SCPH-1200 calibrado com atenuação de 15% (`#6D0000`).
  * **Modo Digital:** Branco suave / cinza ambiente (`#101010`).

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

1. Baixe o pacote **[`BlueRetro_PS1_Custom_v0.5.1_Package.zip`](https://github.com/Schickacka/BlueRetro/releases)** na aba Releases.
2. Ligue o PlayStation 1 com os controles Bluetooth desligados.
3. Execute o arquivo **`flash_bluetooth_ota.bat`** (Windows).
4. O script localizará o dispositivo via Bluetooth LE e efetuará a gravação em aproximadamente 50 segundos. O receptor reiniciará automaticamente com o firmware atualizado.

*Alternativa via Navegador:* Acesse **[blueretro.io/ota.html](https://blueretro.io/ota.html)** no Chrome ou Edge, conecte-se ao BlueRetro e envie o arquivo `BlueRetro_hw1_playstation.bin`.

### Método 2: Gravação via Cabo USB (Windows)
1. Conecte o ESP32 ao computador via cabo micro-USB.
2. Extraia o conteúdo de **`BlueRetro_PS1_Custom_v0.5.1_Package.zip`**.
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

### Comportamento Validado em Jogos
A calibração do firmware abrange os padrões de comunicação e feedback utilizados pela biblioteca do PlayStation:
* **Efeitos de Pulso e Modulação:** Resposta de motores pesados com repouso verdadeiro nos intervalos de baixa intensidade (como batimentos cardíacos, avisos de proximidade e alertas de status).
* **Vibração Sustentada Contínua:** Disparos prolongados de armas automáticas, aceleração contínua e superfícies acidentadas sem interrupções por watchdog.
* **Transições de Cena e Vídeos:** Preservação do modo analógico durante pausas de leitura em FMVs e telas de transição.
* **Resposta de Impacto:** Acionamento agudo em golpes, impactos rápidos e colisões.

---

## Compilação a Partir do Código-Fonte

Este repositório possui fluxo automatizado de integração contínua (CI) via **GitHub Actions** que gera os binários a cada commit na branch `master`. Para compilação local utilizando o framework **ESP-IDF v5.5**:

```bash
# Clonagem recursiva obrigatória para obtenção de submódulos
git clone --recursive https://github.com/Schickacka/BlueRetro.git
cd BlueRetro

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
