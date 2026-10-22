# BlueRetro — PlayStation 1 Custom Edition (v0.7.1)

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

## Novidades e Correções da Versão 0.7.1

A versão 0.7.1 refina o subsistema de feedback da v0.7.0, sanando riscos de corte prematuro na temporização de conexão e eliminando sobrecarga no enlace Bluetooth:

1. **Proteção Integral do Pulso de Conexão de 150 ms:**
   - Adicionada a função [`ps_conn_rumble_active()`](main/bluetooth/hidp/ps.c), que sinaliza o estado ativo da vibração háptica de conexão.
   - Em [`wireless/ps.c`](main/adapter/wireless/ps.c), comandos ociosos de parada (`state == 0`) enviados pelo console e eventos simultâneos de LED são impedidos de cortar os motores precocemente durante o pulso inicial.
   - Se um comando legítimo de vibração do jogo (`state != 0`) chegar durante o pareamento, os dados reais do jogo assumem precedência imediata.

2. **Eliminação de Tráfego Ocioso de Parada a 10 Hz:**
   - Em [`ps_spi.c`](main/wired/ps_spi.c), a rotina de ressincronização periódica a cada 6 frames foi restrita exclusivamente a vibrações ativas (`r != 0 || l != 0`).
   - Quando o console está ocioso (`r == 0 && l == 0`), o reenvio contínuo de pacotes de parada a ~10 Hz foi eliminado, preservando a largura de banda e mantendo o link Bluetooth em repouso.

3. **Ajuste e Rigor na Documentação Técnica:**
   - Esclarecido o modelo de despacho *best-effort* no desligamento/reset em [`host.c`](main/bluetooth/host.c).
   - Identificada a dependência de `CONFIG_BR_PS1_BATTERY_IND=y` para os avisos luminosos de bateria (desativado por padrão no perfil oficial de hardware).
   - Atualizadas as instruções de build local com a geração explícita de `version.txt` e a dependência `bleak` para o gravador OTA.

---

## Novidades e Correções da Versão 0.7.0

A versão 0.7.0 implementa 10 correções arquiteturais profundas na camada de concorrência, filas de feedback e máquina de estados de iluminação e rumble:

1. **Garantia de Parada de Rumble sob Pressão de Fila:**
   - A função `adapter_q_fb()` agora retorna confirmação de inserção na fila (`bool`).
   - Se a fila estiver cheia no momento em que o jogo comanda a parada dos motores, o evento de parada é retido em uma máscara atômica de fallback (`fb_rumble_stop_mask`) e aplicado diretamente pela tarefa `bt_fb_task`, recuperando comandos de parada quando a fila de entrada descarta pacotes sob alta carga em controles PlayStation (DS4 / DS5).
   - O shadow do barramento SPI (`rumble_r_val`/`rumble_l_val`) só é atualizado após enfileiramento bem-sucedido, forçando repetição no próximo ciclo se houver descarte.
   - Adicionada ressincronização periódica a cada ~100 ms para recuperar estados de vibração eventualmente perdidos no ar.

2. **Parada Segura de Motores no Reset ou Desconexão do Dispositivo:**
   - Em `bt_host_reset_dev()`, envio em modo best-effort de relatório de desligamento dos motores antes da liberação de memória e encerramento do driver, mitigando motores presos quando o enlace Bluetooth ainda aceita pacotes antes do reset.

3. **Isolamento de Feedback por Dispositivo (`fb_changed_mask`):**
   - Substituição da variável global `fb_changed` por uma máscara de bits por dispositivo (`fb_changed_mask`), evitando que comandos de LED ou rumble direcionados ao Jogador 1 forcem transmissões desnecessárias de pacotes ao Jogador 2.

4. **Eliminação de Ambiguidade no Pulso de Conexão:**
   - O pulso háptico de 150 ms na conexão agora utiliza um contador de eventos reais do jogo (`game_fb_cnt`). Se o jogo enviar comandos de vibração durante a inicialização, o comando do jogo prevalece e o pulso não sobrescreve os dados legítimos.

5. **Higienização de Motores em Comandos de LED:**
   - Ao processar eventos de LED (`FB_TYPE_PLAYER_LED` / `FB_TYPE_STATUS_LED`), os motores são explicitamente zerados caso o flag de rumble esteja desligado, impedindo que dados residuais no buffer reativem a vibração.

6. **Configuração Unificada de Cores via Kconfig:**
   - A opção `CONFIG_BR_PS1_LED_DIGITAL_COLOR` agora rege oficialmente a cor do DualShock 4 (padrão `#949190`).
   - Criada a opção `CONFIG_BR_PS1_LED_DIGITAL_COLOR_DS5` (padrão `0x000000`), permitindo personalização independente mantendo o DualSense com a barra apagada e apenas os pontos de jogador ativos.

7. **Preservação de Rumble Durante Avisos de Bateria:**
   - Quando a indicação de bateria baixa estiver ativada (`CONFIG_BR_PS1_BATTERY_IND=y`, opcional), a rotina `ps_send_led_only()` preserva os parâmetros ativos de vibração em vez de zerar os motores a cada pulso luminoso.

8. **Retenção de Cor de Jogador Durante Animação de Conexão:**
   - Se um comando de LED de jogador chegar enquanto o controle executa a respiração azul inicial, a cor solicitada é registrada como pendente (`ps4_set_pending_led`) e aplicada imediatamente ao término da animação.

9. **Telemetria e Diagnóstico Avançado:**
   - Registro detalhado e legível no log de inicialização do motivo de reset do ESP32 (`esp_reset_reason()`), facilitando a identificação imediata de subtensão (*brownout*), *panic* ou *watchdog*.
   - Registro do código de motivo nos eventos de desconexão Bluetooth HCI (`reason: 0xXX`).
   - Registro de transições de vibração e contagem de eventos descartados de fila limitados a 1 linha/segundo para não concorrer com o lock de transmissão.

10. **Proteção Contra Underflow no Parser HID:**
    - Validação rígida do comprimento mínimo de pacotes em `bt_hid_ps_hdlr()`, descartando com segurança pacotes truncados e prevenindo corrupção de memória.

---

## Novidades e Correções da Versão 0.6.12

* **Ajuste de Brilho no DualShock 4 (-25%):**
  - O valor de cinza neutro do DualShock 4 foi ajustado de `#c5c1c0` para `#949190` (`0x00909194` em formato little-endian `0x00BBGGRR`), reduzindo o brilho em exatamente 25% para visualização confortável, economia de bateria e redução drástica do consumo de corrente elétrica dos LEDs.

* **Restauração da Estabilidade de Conexão (Linha de Base v0.6.4):**
  - **Probe Escuro na Inicialização (`bt_hid_ps_init`):** Retornado o valor de `leds = 0x000000` nos pacotes de sondagem inicial (`ps4_set_conf`), idêntico à versão estável 0.6.4. Evita que o controle acione alta corrente de LED durante a negociação dos canais L2CAP/HCI Bluetooth, eliminando quedas de tensão na bateria e desconexões espúrias do DS4.
  - **Eliminação de Pacote Duplicado de Gatilhos:** Removido o reenvio redundante `bt_hid_cmd_ps5_set_conf` após `bt_hid_cmd_ps5_trigger_init`, aliviando o tráfego de saída do canal de interrupção na conexão.
  - **Sequência de Conexão Fluida:** O DS4 conecta de forma suave e silenciosa, executa a respiração azul indicadora de slot (Player 1 / Player 2) e, ao término, acende suavemente no cinza calibrado `#949190` no Modo Digital. No Modo Analógico, transita imediatamente para o Vermelho autêntico (`0x00006D`). O DualSense preserva seu comportamento 100% calibrado da v0.6.10/v0.6.11.

---

## Novidades e Correções da Versão 0.6.11

* **Iluminação Especializada DualShock 4 vs DualSense (PS5):**
  - **DualShock 4 com LED Cinza Neutro (#c5c1c0):** Como o controle DualShock 4 não possui indicadores luminosos de jogador independentes da barra de luz, a lightbar permanece acesa em cinza neutro suave (`#c5c1c0` / `0xC0C1C5`) durante a conexão no Modo Digital, permitindo identificar visualmente que o controle está ligado e conectado. No Modo Analógico, a barra transita para Vermelho autêntico (`0x00006D`).
  - **DualSense (PS5) Mantido com Lightbar Desligada no Digital:** O DualSense permanece com o comportamento perfeito aprovado na v0.6.10: lightbar 100% desligada no Modo Digital com apenas os pontos discretos de identificação de jogador acesos sob o touchpad (1 ponto = P1, 2 pontos = P2), acendendo em Vermelho no Modo Analógico.

---

## Novidades e Correções da Versão 0.6.10

* **Modo Digital Autêntico SCPH-1200 (Lightbar Desligada):**
  - **Fidelidade Histórica ao DualShock Original:** No controle SCPH-1200 do PlayStation 1, o LED vermelho só acendia quando o modo Analógico estava ativo; no modo Digital, permanecia 100% apagado. A cor padrão do modo Digital foi definida como `0x000000` (desligada) para DS4 e DS5.
  - **Indicação de Conexão e Jogador Preservadas:** O DS4 executa a respiração pulsante azul indicando o número do slot e depois apaga; o DS5 exibe os pontos discretos brancos sob o touchpad (1 ponto para Player 1, 2 pontos para Player 2) sem iluminação residual na lightbar.
  - **Ativação Vermelha em Modo Analógico:** Ao ativar o modo Analógico (seja pelo botão PS ou automaticamente por jogos como Resident Evil 3), a barra luminosa acende instantaneamente no Vermelho autêntico (`0x00006D`) em ambos os controles. Ao desativar, retorna ao estado apagado.

* **Handshake de Reset Linux Kernel (`dualsense_reset_leds`):**
  - **Cancelamento da Animação Azul de Boot do DualSense:** Ao conectar via Bluetooth, o DualSense executa autonomamente uma animação de boot azul e mantém a cor se não for comandado a sair. Alinhado ao driver oficial da Sony no Linux (`hid-playstation`), implementado o envio de pacote de reset de lightbar com `valid_flag2 = 0x02` (`LIGHTBAR_SETUP_CONTROL_ENABLE`) e `lightbar_setup = 0x02` (`LIGHT_OUT`) no callback de inicialização.
  - **Isolamento de Modo Operacional:** Os relatórios de feedback e atualização subsequentes (`ps5_fb_from_generic`, `ps_send_led_only`, `bt_hid_ps5_init_callback`) mantêm `valid_flag2 = 0x00` e `lightbar_setup = 0x00`, permitindo que o controle aplique livremente os comandos de cor RGB e os LEDs indicadores de jogador sem conflito com o registrador de setup.

---

## Novidades e Correções da Versão 0.6.9

* **Correção Definitiva da Lightbar do DualSense (PS5):**
  - **Reversão do Macro `lightbar_setup` (`LIGHT_ON` = `0x01`):** No relatório Bluetooth `0x31` do DualSense, a constante oficial do driver Sony Linux (`hid-playstation`) define `DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_ON = 0x01` (`BIT(0)`) e `DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT = 0x02` (`BIT(1)`). Na versão 0.6.8, `lightbar_setup` estava transmitindo `0x02` com o flag de validação `valid_flag2 = 0x02`, comandando ativamente o controle a desligar a barra luminosa (`LIGHT_OUT`). Corrigido para `BT_HIDP_PS5_LIGHTBAR_SETUP_ON = 0x01` em todos os pontos de saída (`ps5_fb_from_generic`, `bt_hid_ps5_init_callback`, `ps_send_led_only`).
  - **Calibração Luminosa do Modo Digital:** O valor padrão de cor digital foi ajustado para `0x202020` (RGB 32, 32, 32), proporcionando um brilho branco/cinza suave nítido e perfeitamente visível através dos difusores ópticos do DualSense, contrastando imediatamente com o Vermelho (`0x00006D`) ativado no modo Analógico em jogos como Resident Evil 3.

---

## Novidades e Correções da Versão 0.6.8

* **Auditoria DeepSeek e Correção Completa do Protocolo DualSense (PS5):**
  - **Restauração dos Valores de Configuração da Lightbar:** Corrigido o valor de `BT_HIDP_PS5_LIGHTBAR_SETUP_ON` para `0x02` e `valid_flag2` para `0x02` em todos os relatórios (`ps5_fb_from_generic`, `bt_hid_ps5_init_callback`, `ps_send_led_only`). A inversão anterior (`0x01`) comandava o controle a apagar a barra de luz. A lightbar agora acende confiavelmente em Branco suave no modo Digital (`0x101010`) e Vermelho no modo Analógico (`0x00006D`).
  - **Preservação de Estado em `bt_hid_cmd_ps5_trigger_init`:** O comando de inicialização de gatilhos adaptativos não mais inicializa uma estrutura vazia que zerava a configuração luminosa (`conf1`, `valid_flag2`, `player_leds`, `leds`). Agora herda o estado de saída ativo (`*base_conf`) e reenvia a configuração LED logo após a aplicação dos parâmetros hápticos.
  - **Eliminação de Vibração Infinita no Pulso de Conexão:** No tratador de motor (`ps_start_player_rumble` e `bt_hid_ps_tick`), garantida a transmissão explícita do flag `cmd = 0x03` (`valid_flag0 & 0x01`) ao término dos 150 ms de pulso. Sem esse bit ativo, o firmware do DualSense descartava comandos de parada e mantinha os motores girando indefinidamente. Adicionalmente, `saved_hf`/`saved_lf` agora recebem salvaguarda para não capturarem o pulso transitório como linha de base.
  - **Higienização de Buffers na Conexão e Transição Multi-Controle:** Inserida limpeza por `memset` da estrutura completa de 77 bytes (`struct bt_hidp_ps5_set_conf`) tanto em `bt_hid_ps_init` quanto no evento de reclassificação `BT_HIDP_PS5_STATUS` em `bt_hid_ps_hdlr`. Evita que dados residuais de DualShock 4 interfiram no alinhamento de relatórios do DualSense em conexões subsequentes.
  - **Sequenciamento Correto de LEDs na Inicialização:** Restaurado o envio único do pacote `ps5_clear_led` (`conf1 = 0x08`, `RELEASE_LEDS`) exclusivamente no callback de inicialização `bt_hid_ps5_init_callback` para liberar a animação interna de boot do controle, mantendo a ausência de interrupções durante o jogo e ao acionar o botão PS.

---

## Novidades e Correções da Versão 0.6.7

* **Correção Definitiva da Lightbar e LEDs do DualSense (PS5):**
  - **Correção da Flag `lightbar_setup` (`LIGHT_ON`):** O comando `lightbar_setup` em todos os relatórios DualSense (`ps5_fb_from_generic`, `bt_hid_ps5_init_callback` e `ps_send_led_only`) estava erroneamente configurado como `0x02` (`DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT`), que comanda o microcontrolador do controle a apagar/esmaecer a lightbar. O valor foi corrigido para `0x01` (`DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_ON`), mantendo a lightbar permanentemente iluminada na cor correspondente (Branco suave para modo Digital, Vermelho para Analógico).
  - **Habilitação Conjunta de Brilho e Lightbar (`valid_flag2 = 0x03`):** O campo `valid_flag2` foi ajustado para habilitar tanto o controle de brilho (`0x01`) quanto a configuração da lightbar (`0x02`), garantindo que a escala de intensidade noturna e a cor RGB sejam plenamente reconhecidas pelo controle.
  - **Eliminação do Piscar Indevido nos Indicadores de Jogador:** Removido o envio do pacote transitório `ps5_clear_led` (`conf1 = 0x08`, `RELEASE_LEDS`) em `bt_fb_task`. Ao alternar entre modo Digital e Analógico pelo botão PS/Home, os LEDs de indicação de jogador permanecem fixos e estáveis sem piscar ou apagar momentaneamente.
  - **Estabilidade em Ambientes Multi-Controle (DS4 + DS5):** Corrigido o retorno booleano de `ps5_fb_from_generic` e eliminada a contenção de relatórios, garantindo que o DualSense no Slot 2 responda imediatamente às transições de modo e mantenha seus LEDs acesos mesmo após desconexões ou conexões prévias de DualShock 4 no Slot 1.

---

## Novidades e Correções da Versão 0.6.6

* **Correção da Lightbar e LEDs de Conexão do DualSense (PS5):**
  - **Restauração da Cor da Lightbar em `FB_TYPE_PLAYER_LED` e Guarda Ativa:** No tratador `ps5_fb_from_generic`, o evento de mapeamento de porta `FB_TYPE_PLAYER_LED` não atribuía a cor da lightbar para o modo emulado (Digital/Analógico), permitindo que a barra ficasse apagada após eventos de conexão ou alternância entre controles. O tratador agora calcula e mantém a cor ativa (Branco suave em Digital / Vermelho em Analógico) e conta com uma guarda para garantir que `set_conf->leds` nunca seja transmitido como zero.
  - **Identificação Precisa do Subtipo DualSense:** No tratador de status `BT_HIDP_PS5_STATUS` e no callback de inicialização, `device->ids.subtype = BT_PS5_DS` é explicitamente atribuído, garantindo o roteamento correto de relatórios de feedback e pacotes `0x31`.
  - **Preservação do Ciclo Nativo de Inicialização e Pacote de Liberação de LEDs:** Mantido o temporizador de inicialização de 1 segundo para estabilização de canais Bluetooth e preservado o pacote `ps5_clear_led` (`conf1 = 0x08`, `RELEASE_LEDS`) exigido pelo firmware do DualSense antes da escrita de novos estados luminosos.

---

## Novidades e Correções da Versão 0.6.4

* **Desativação Padrão do Indicador de Bateria (`CONFIG_BR_PS1_BATTERY_IND=n`):** A opção foi desativada por precaução e para eliminar qualquer relatório LED-only de bateria. No DualShock 4 (e no caminho de clones em status HID), o relatório LED-only usava `conf1 = 0x03` com motores zerados, apresentando risco de corte de vibração. No DualSense, o relatório utiliza `cmd = 0x00`/`conf1 = 0x14`, sem habilitar os motores. A leitura interna e o gerenciamento de carga permanecem ativos no próprio controle.
* **Correção no Temporizador de Inicialização do Switch / 8BitDo (`sw.c`):** Corrigida falha no callback `bt_hid_sw_init_callback()`, onde o timer era deletado prematuramente antes da chamada `esp_timer_start_once(device->timer_hdl, 100000)` com ponteiro nulo (`ESP_ERR_INVALID_ARG`), impedindo retentativas em caso de perda de pacotes de handshake. O timer ativo agora é preservado e reiniciado com segurança.
* **Guarda Analógica Reforçada em Hotplug e Reset do Console (`analog_lock`):** A trava de liberação do botão PS/Home foi reforçada para ser rearmada na inicialização da porta (`ps_spi_port_cfg`) e durante o reset ocioso caso o botão físico ainda esteja mantido pressionado (`output[18] != 0`), impedindo alternâncias espúrias e involuntárias para o modo analógico (`0x73`) ao ligar ou resetar o console.
* **Rumble em Modo Digital Preservado:** O gate do comando `0x42` processa o feedback quando `dev_type == DEV_PS2_DS2` (todos os `DEV_PAD`); mouse e flight, que são `!= DEV_PS2_DS2`, ficam de fora. Isso assegura que jogos que enviam vibração em modo digital (`0x41`) funcionem normalmente.
* **Preservação Integral do Comportamento Validado:** Mantido o boot em modo digital (`0x41`), a comutação de analógico exclusivamente no release do botão PS/Home, o chaveamento automático de analógico pelos jogos (`0x44`) e a desativação do Memory Card Virtual (`CONFIG_BR_PS1_MEMORY_CARD=n`).

---

## Novidades e Correções da Versão 0.6.3

* **Ativação Padrão do Indicador de Bateria Baixa (`CONFIG_BR_PS1_BATTERY_IND=y`):** O indicador luminoso de bateria fraca passa a ser ativado por padrão na compilação oficial. Em controles DualShock 4 e DualSense com nível de carga crítico (<= 20%) e sem cabo de carregamento conectado, a barra luminosa emite um pulso âmbar suave (`0x000080FF`) a cada 30 segundos. *(Nota da v0.6.4: o indicador foi desativado na versão seguinte; a afirmação de ausência de interferência no rumble não havia sido validada em hardware e há risco de corte de vibração no DS4).*
* **Confirmação de Desativação do Memory Card Virtual (`CONFIG_BR_PS1_MEMORY_CARD=n`):** Mantido desativado por padrão conforme especificação do projeto para adaptador dedicado de controles, preservando 128 KB de heap DMA livre para conexões Bluetooth simultâneas e máxima estabilidade.

---

## Novidades e Correções da Versão 0.6.2

* **Rumble Ativo em Modo Digital no PS1 (`0x41`):** Correção do gate de vibração em `ps_spi.c` para processar feedback quando `dev_type == DEV_PS2_DS2` (todos os `DEV_PAD`), excluindo mouse e flight (`!= DEV_PS2_DS2`). Isso preserva o comportamento autêntico do fork de permitir vibração mesmo quando o PlayStation 1 ainda enxerga o controle como digital (`0x41`), antes da alternância para analógico (`0x73`) pelo jogo ou pelo botão PS/Analog.
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

> [!NOTE]
> O script de atualização sem fio (`ble_ota_update.py`) requer a biblioteca Python `bleak` (`pip install bleak`).

1. Baixe o pacote **[`BlueRetro_PS1_Custom_v0.7.1_Package.zip`](https://github.com/Schickacka/BlueRetro/releases)** na aba Releases.
2. Ligue o PlayStation 1 com os controles Bluetooth desligados.
3. Execute o arquivo **`flash_bluetooth_ota.bat`** (Windows).
4. O script localizará o dispositivo via Bluetooth LE e efetuará a gravação em aproximadamente 50 segundos. O receptor reiniciará automaticamente com o firmware atualizado.

*Alternativa via Navegador:* Acesse **[blueretro.io/ota.html](https://blueretro.io/ota.html)** no Chrome ou Edge, conecte-se ao BlueRetro e envie o arquivo `BlueRetro_hw1_playstation.bin`.

### Método 2: Gravação via Cabo USB (Windows)
1. Conecte o ESP32 ao computador via cabo micro-USB.
2. Extraia o conteúdo de **`BlueRetro_PS1_Custom_v0.7.1_Package.zip`**.
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

# Gerar arquivo de versão do firmware customizado
echo "v0.7.1 ps1-custom" > version.txt

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
