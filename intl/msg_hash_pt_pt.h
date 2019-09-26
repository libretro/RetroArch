#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

MSG_HASH(
      MSG_COMPILER,
      "Compilador"
      )
MSG_HASH(
      MSG_UNKNOWN_COMPILER,
      "Compilador desconhecido"
      )
MSG_HASH(
      MSG_NATIVE,
      "Native"
      )
MSG_HASH(
      MSG_DEVICE_DISCONNECTED_FROM_PORT,
      "Dispositivo desconectado da porta"
      )
MSG_HASH(
      MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
      "Recebido comando Netplay desconhecido"
      )
MSG_HASH(
      MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
      "O ficheiro já existe. A guardar no buffer de cópia de segurança"
      )
MSG_HASH(
      MSG_GOT_CONNECTION_FROM,
      "Ligaçao obtida de: \"%s\""
      )
MSG_HASH(
      MSG_GOT_CONNECTION_FROM_NAME,
      "Ligação obtida de: \"%s (%s)\""
      )
MSG_HASH(
      MSG_PUBLIC_ADDRESS,
      "Endereço público"
      )
MSG_HASH(
      MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
      "Não foi fornecido qualquer argumento e nenhum menu está contido. Mostrando ajuda..."
      )
MSG_HASH(
      MSG_SETTING_DISK_IN_TRAY,
      "Colocando o disco na área de notificação"
      )
MSG_HASH(
      MSG_WAITING_FOR_CLIENT,
      "Aguardando pelo cliente ..."
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
      "Você saiu do jogo"
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
      "Você juntou-se como o(a) jogador(a) %u"
      )
MSG_HASH(
      MSG_NETPLAY_ENDIAN_DEPENDENT,
      "Este núcleo não suporta inter-arquitetura de Netplay entre estes sistemas"
      )
MSG_HASH(
      MSG_NETPLAY_PLATFORM_DEPENDENT,
      "Este núcleo não suporta inter-arquitetura de Netplay"
      )
MSG_HASH(
      MSG_NETPLAY_ENTER_PASSWORD,
      "Introduza a palavra-passe do servidor:"
      )
MSG_HASH(
      MSG_NETPLAY_INCORRECT_PASSWORD,
      "Palavra-passe incorreta"
      )
MSG_HASH(
      MSG_NETPLAY_SERVER_NAMED_HANGUP,
      "\"%s\" terminou a ligação ao servidor de Netplay"
      )
MSG_HASH(
      MSG_NETPLAY_SERVER_HANGUP,
      "O cliente Netplay foi desligado"
      )
MSG_HASH(
      MSG_NETPLAY_CLIENT_HANGUP,
      "Desligado do Netplay"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
      "Não tem permissão para jogar"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
      "Não existem posições livres para jogadores"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY,
      "Não é possível entrar no modo de reprodução"
      )
MSG_HASH(
      MSG_NETPLAY_PEER_PAUSED,
      "O parceiro \"%s\" da sessão de Netplay pausou o jogo"
      )
MSG_HASH(
      MSG_NETPLAY_CHANGED_NICK,
      "A sua alcunha foi alterada para \"%s\""
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
      "Dê aos núcleos renderizados por hardware o seu próprio contexto privado. Evita ter que assumir mudanças de estado de hardware entre frames."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
      "Enable horizontal animation for the menu. This will have a performance hit."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_SETTINGS,
      "ajusta as definições de aparência no ecrã do menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
      "Sincroniza o hardware do processador e da GPU. Reduz a latência, com um custo no desempenho."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_THREADED,
      "Melhora o desempenho, a troco de latência e engasgamento do vídeo. Utilize apenas esta funcionalidade, caso não consiga obter um desempenho pleno através de outro método."
      )
MSG_HASH(
      MSG_AUDIO_VOLUME,
      "Volume de som"
      )
MSG_HASH(
      MSG_AUTODETECT,
      "Auto-detetar"
      )
MSG_HASH(
      MSG_AUTOLOADING_SAVESTATE_FROM,
      "Auto-carregar estado da gravação de"
      )
MSG_HASH(
      MSG_CAPABILITIES,
      "Funcionalidades"
      )
MSG_HASH(
      MSG_CONNECTING_TO_NETPLAY_HOST,
      "Ligando ao anfitrião de Netplay"
      )
MSG_HASH(
      MSG_CONNECTING_TO_PORT,
      "Ligando à porta"
      )
MSG_HASH(
      MSG_CONNECTION_SLOT,
      "Vaga para ligação"
      )
MSG_HASH(
      MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
      "Lamentamos, esta funcionalidade não implementada: os núcleos que não exigem conteúdo não podem participar no Netplay."
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
      "Palavra-passe"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
      "Definições de contas Cheevos"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
      "Nome de utilizador"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
      "Conta"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
      "Nó da lista de contas"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
      "Conquistas Retro"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
      "Lista de conquistas"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
      "Lista de conquistas (Hardcore)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
      "Procurar conteúdo"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
      "Configurações"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ADD_TAB,
      "Importar conteúdo"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
      "Salas de Netplay"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
      "Perguntar"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
      "Recursos"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
      "Bloquear fotogramas"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
      "Dispositivo de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
      "Controlador de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
      "Plugin de som DSP"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
      "Ativar som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
      "Filtro de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
      "Turbo/Zona-morta"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
      "Latência de som (ms)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
      "Otimização de tempo máximo de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
      "Silenciar som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
      "Taxa de saída de som (Hz)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
      "Controlo dinâmico de taxa de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
      "Controlador de reamostragem de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
      "Definições de som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
      "Sincronizar som"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
      "Nível de volume de som (dB)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
      "Intervalo de auto-gravação de SaveRAM"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
      "Carregar ficheiros de substituição automaticamente"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
      "Carregar os ficheiros de mapeamento automaticamente"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
      "Carregar os shaders automaticamente"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
      "Voltar"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
      "Confirmar"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
      "Informações"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
      "Sair"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
      "Deslizar para baixo"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
      "Deslizar para cima"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
      "Iniciar"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
      "Ativar/Desativar teclado"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
      "Ativar/Desativar menu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
      "Controlos básicos do menu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
      "Confirmar/OK"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
      "Informações"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
      "Sair"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
      "Deslizar para cima"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
      "Pré-definições"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
      "Ativar/Desativar teclado"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
      "Ativar/Desativar menu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
      "Não substituir o SaveRAM no carregamento do estado de gravação"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
      "Ativar Bluetooth"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
      "URL dos recursos do buildbot"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
      "Cache"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
      "Permitir câmera"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
      "Controlador de câmera"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT,
      "Batota"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
      "Aplicar alterações"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
      "Caminho da base de dados de batota"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
      "Ficheiro de batota"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
      "Carregar ficheiro de batota"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
      "Gravar ficheiro de batota como"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
      "Passagens de batota"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
      "Descrição"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Conquistas no Modo Hardcore"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS,
      "Conquistas bloqueadas:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
      "Bloqueada"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
      "Conquistas Retro"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
      "Testar conquista não oficial"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS,
      "Conquistas desbloqueadas:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
      "Desbloqueada"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
      "Fechar conteúdo"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIG,
      "Configurar"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
      "Carregar configuração"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
      "Configuração"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
      "Guardar configuração ao sair"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
      "Base de dados"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
      "Conteúdo"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
      "Tamanho do histórico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
      "Permitir a remoção de entradas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
      "Menu rápido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
      "Transferências")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
      "Transferências")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
      "Batota")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
      "Contadores dos núcleos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
      "Mostrar nome do núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
      "Informação do núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
      "Autores")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
      "Categorias")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
      "Designação do núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
      "Nome do núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
      "Firmware(s)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
      "Licença(s)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
      "Permissões")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
      "Extensões suportadas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
      "Fabricante do sistema")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
      "Nome do sistema")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
      "Mapeamento de teclas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_LIST,
      "Carregar núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
      "Opções")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
      "Núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
      "Iniciar um núcleo automaticamente")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "Extrair automaticamente o ficheiro transferido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
      "URL dos núcleos do buildbot")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
      "Atualizador de núcleos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
      "Atualizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
      "Arquitetura do processador:")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CPU_CORES,
      "Núcleo do processador:")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
      "Cursor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
      "Gestor do cursor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
      "Rácio personalizado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
      "Gestor de base de dados")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
      "Seleção de base de dados")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
      "Remover")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FAVORITES,
      "Iniciar pasta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
      "<Conteúdo da pasta>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
      "<Pré-definição>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
      "<Nenhum>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
      "Pasta não encontrada.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
      "Pasta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS,
      "Estado do ciclo do disco na área de notificação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
      "Adicionar imagem de disco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_INDEX,
      "Índice do disco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
      "Controlo de disco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DONT_CARE,
      "Ignorar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
      "Transferências")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
      "Transferir núcleo...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
      "Descarregador de conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE,
      "Ativar sobreposição de DPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE,
      "Sobreposição de DPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
      "Controlador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
      "Carregar conteúdo vazio no encerramento do núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
      "Verificar por firmware em falta antes do carregamento de conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
      "Fundo dinâmico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
      "Fundos dinâmicos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
      "Ativar conquistas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FALSE,
      "Falso")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
      "Executar em velocidade máxima")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_SHOW,
      "Mostrar taxa de fotogramas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
      "Limitar velocidade máxima de execução")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
      "Aceleração de fotogramas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
      "Contadores da interface visual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
      "Carregar opções específicas de conteúdos de núcleos automaticamente ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
      "Criar ficheiro de opções de jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
      "Ficheiro de opções de jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP,
      "Ajuda")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
      "Solução de problemas de som/vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
      "Modificando a sobreposição do comando virtual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
      "Controlos principais do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_LIST,
      "Ajuda")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
      "Carregando Conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
      "Procurando por conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
      "O que é um núcleo?")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
      "Ativar histórico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
      "Histórico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
      "Menu horizontal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
      "Imagem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION,
      "Informação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
      "Informação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
      "Tipo analógico p/ digital")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
      "Menu de teclas de todos os utilizadores")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
      "Analógico esquerdo X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
      "Analógico esquerdo X- (esquerda)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
      "Analógico esquerdo X+ (direita)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
      "Analógico esquerdo Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
      "Analógico esquerdo Y- (cima)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
      "Analógico esquerdo Y+ (baixo)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
      "Analógico direito X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
      "Analógico direito X- (esquerda)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
      "Analógico direito X+ (direita)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
      "Analógico direito Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
      "Analógico direito Y- (cima)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
      "Analógico direito Y+ (baixo)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
      "Gatilho pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
      "Recarregar pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
      "Pistola Auxiliar A")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
      "Pistola Auxiliar B")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
      "Pistola Auxiliar C")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
      "Start da pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
      "Select da pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
      "Botão direcional (cima) da pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
      "Botão direcional (baixo) da pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
      "Botão direcional (esquerda) da pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
      "Botão direcional (direita) da pistola")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
      "Ativar auto-configuração de teclas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
      "Menu trocar botões OK e Cancelar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
      "Associar todas as teclas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
      "Associar todas as teclas pré-definidas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
      "Tempo limite para associação de teclas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
      "Esconder descritores de núcleo não consolidados")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
      "Mostrar designações do descritor de entrada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
      "Índice do dispositivo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
      "Tipo de dispositivo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
      "Controlador de entrada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
      "Ciclo de trabalho")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
      "Associação de tecla de atalho")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
      "Ativar mapeamento do comando no teclado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
      "Botão A (direita)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
      "Botão B (baixo)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
      "Botão direcional (baixo)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
      "Botão L2 (gatilho)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
      "Botão L3 (polegar)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
      "Botão L (ombro)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
      "Botão direcional (esquerda)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
      "Botão R2 (gatilho)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
      "Botão R3 (polegar)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
      "Botão R (ombro)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
      "Botão direcional (direito)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
      "Botão Select")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
      "Botão Start")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
      "Botão direcional (cima)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
      "Botão X (topo)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
      "Botão Y (esquerda)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_KEY,
      "Tecla: %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
      "Rato 1")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
      "Rato 2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
      "Rato 3")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
      "Rato 4")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
      "Rato 5")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
      "Roda do rato (cima)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
      "Roda do rato (baixo)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
      "Roda do rato horizontal (esquerda)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
      "Roda do rato horizontal (direita)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
      "Tipo de mapeamento do comando no Teclado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
      "Número máximo de utilizadores")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "Mostrar/esconder menu de controlo do comando")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
      "Índice de batota -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
      "Índice de batota +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
      "Ativar/desativar batota")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
      "Ejetar/recolher disco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
      "Próximo disco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
      "Disco anterior")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
      "Ativar tecla de atalho")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
      "Aguardar pela função de avanço-rápido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
      "Ativar/desativar função de avanço-rápido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
      "Salto de fotogramas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
      "Ativar/desativar ecrã completo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
      "Alterar ponteiro do rato")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
      "Ganhar/perder o foco do jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
      "Carregar estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
      "Mostrar/esconder menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
      "Iniciar/parar gravação de filme")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
      "Silenciar som/remover silêncio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
      "Ativar/desativar modo jogar/olhar no Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
      "Mostrar/esconder teclado no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
      "Próximo overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
      "Ativar/desativar pausa")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
      "Sair do RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
      "Reiniciar jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
      "Função de retrocedimento de tempo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
      "Gravação de estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
      "Captura de ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
      "Shader seguinte")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
      "Shader anterior")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
      "Função de câmera-lenta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
      "Posição de gravação de estado -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
      "Posição de gravação de estado +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
      "Volume -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
      "Volume +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
      "Ativar apresentação de overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
      "Esconder overlay quando o menu estiver ativo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
      "Comportamento do polling de entrada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
      "Mais cedo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
      "Mais tarde")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
      "Normal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
      "Preferir toque frontal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
      "Pasta de mapeamento de entrada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
      "Ativar remapeamento de teclas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
      "Guardar auto-configuração")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
      "Entrada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
      "Ativar teclado pequeno")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
      "Ativar função tátil")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
      "Ativar Turbo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
      "Período do turbo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
      "Associar as teclas do utilizador %u")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
      "Estado do armazenamento interno")
MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
      "Entrada de auto-configuração")
MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
      "Controlador de comandos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
      "Serviços")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
      "Chinês (Simplificado)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
      "Chinês (Tradicional)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
      "Holandês")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
      "Inglês")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
      "Esperanto")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
      "Francês")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
      "Alemão")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
      "Italiano")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
      "Japonês")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
      "Coreano")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_POLISH,
      "Polaco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
      "Português (Brasileiro)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
      "Português (Portugal)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
      "Russo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
      "Espanhol")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
      "Vietnamita")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
      "Árabe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GREEK,
      "Grego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
      "Turco")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
      "Analógico esquerdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
      "Núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
      "Informação do núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
      "Nível de registo principal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LINEAR,
      "Linear")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
      "Carregar ficheiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
      "Carregar conteúdo recente")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
      "Carregar conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_DISC,
      "Load Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMP_DISC,
      "Dump Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_STATE,
      "Carregar estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
      "Permitir localização")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
      "Controlador de localização")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
      "Entrando")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
      "Verbosidade do registo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MAIN_MENU,
      "Menu principal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MANAGEMENT,
      "Definições da base de dados")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
      "Tema da cor do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
      "Azul")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
      "Azul acizentado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
      "Azul escuro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
      "Verde")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
      "NVIDIA Shield")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
      "Vermelho")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
      "Amarelo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
      "Opacidade do rodapé")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
      "Opacidade do cabeçalho")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
      "Controlador de menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
      "Acelerar taxa de fotogramas do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
      "File Browser")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
      "Filtro linear do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
      "Horizontal Animation")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
      "Aparência")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
      "Fundo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
      "Opacidade da imagem de fundo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MISSING,
      "Em falta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MORE,
      "...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
      "Suporte de rato")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
      "Multimédia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
      "Música")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "Extensões de filtro desconhecidas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
      "Envolver em torno da navegação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NEAREST,
      "O mais próximo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY,
      "Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
      "Verificação dos fotogramas de Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
      "Latência de entrada dos fotogramas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
      "Intervalo de latência de entrada dos fotogramas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
      "Atraso de fotogramas do Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
      "Terminar ligação ao anfitrião de Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
      "Ativar Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
      "Ligar a um anfitrião de Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
      "Iniciar sessão de anfitrião de Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
      "Parar anfitrião de Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
      "Endereço do servidor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
      "Procurar na rede local")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
      "Permitir cliente de Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
      "Utilizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
      "Palavra-passe do servidor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
      "Anunciar o Netplay de forma pública")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
      "Definições do Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
      "Modo sem estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
      "Palavra-passe do servidor para espectador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
      "Ativar modo de espectador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
      "Porta TCP")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
      "NAT transversal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
      "Comandos do rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
      "Porta de comando de rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
      "Informação de rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
      "Jogo em rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
      "Porta de rede remota")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
      "Rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO,
      "Não")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NONE,
      "Nenhum")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
      "N/D")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
      "Não existem conquistas para serem exibidas.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE,
      "Nenhum núcleo se encontra selecionado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
      "Não existem núcleos disponíveis.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
      "Não existe informação do núcleo disponível.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
      "Não existem opções de núcleo disponíveis.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
      "Não existem entradas para serem mostradas.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
      "Sem histórico disponível.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
      "Não há informação disponível.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ITEMS,
      "Sem itens.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
      "Nenhum anfitrião de Netplay encontrado.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
      "Nenhuma rede encontrada.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
      "Não existem contadores de desempenho.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
      "Não existem listas de reprodução.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
      "Não existem entradas na lista de reprodução.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
      "Não foram encontras definições.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
      "Não há parâmetros de shading.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OFF,
      "DESLIGADO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ON,
      "LIGADO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE,
      "On-line")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
      "Atualizador online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
      "Apresentação no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
      "Overlays no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
      "Notificações no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
      "Abrir um ficheiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OPTIONAL,
      "Opcional")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY,
      "Overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
      "Preferência de auto-carregamento de overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
      "Pasta do overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
      "Opacidade do overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
      "Overlay pré-definido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
      "Escala de overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
      "Overlay em ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
      "Usar Modo PAL60")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
      "Diretório pai")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
      "Pausar quando o menu for exibido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
      "Não executar em segundo plano")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
      "Contadores de desempenhp")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
      "Selecionar de Listas de Reprodução")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
      "Pasta de listas de reprodução")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
      "Definições de listas de reprodução")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Label Display Mode"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Change how the content labels are displayed in this playlist."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
    "Show full labels"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
    "Remove () content"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
    "Remove [] content"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
    "Remove () and []"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
    "Keep region"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
    "Keep disc index"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
    "Keep region and disc index"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
      "Suporte de ponteiros")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PORT,
      "Porta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PRESENT,
      "Atual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
      "Privacidade")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
      "Sair do RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
      "Analógico suportado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
      "Classificação BBFC")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
      "Classificação CERO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
      "Modo cooperativo suportado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
      "CRC32")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
      "Descrição")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
      "Estúdio de desenvolvimento")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
      "Edição da revista Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
      "Classificação da revista Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
      "Revisão da revista Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
      "Classificação ELSPA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
      "Melhorias por Hardware")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
      "Classificação ESRB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
      "Classificação da revista Famitsu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
      "Franquia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
      "Género")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
      "MD5")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
      "Nome")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
      "Orígem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
      "Classificação PEGI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
      "Editor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
      "Mês de lançamento")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
      "Ano de lançamento")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
      "Suporte de vibração")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
      "Série")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
      "SHA1")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
      "Iniciar conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
      "Classificação TGDB")
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
      "Reinicializar (RCM)")
#else
MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
      "Reinicializar")
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
      "Pasta de armazenamento das configuração de gravação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
      "Pasta de armazenamento das gravações")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
      "Gravação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
      "Carregar configuração de gravação...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
      "Controlador de gravação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
      "Permitir gravação de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_PATH,
      "Guardar gravações de vídeo em...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
      "Guardar gravações de vídeo na pasta de saída")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE,
      "Remapear ficheiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
      "Carregar ficheiro de remapeamento")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
      "Guardar ficheiro de remapeamento de núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
      "Guardar ficheiro de remapeamento de jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REQUIRED,
      "Obrigatório")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
      "Reiniciar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
      "Reiniciar RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME,
      "Retomar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
      "Retomar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
      "RetroKeyboard")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD,
      "RetroPad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
      "RetroPad com analógico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
      "Conquistas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
      "Ativar função de retrocedimento do tempo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
      "Granularidade da função de retrocedimento do tempo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
      "Função de retrocedimento do tempo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
      "Explorador de ficheiros")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
      "Configuração")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
      "Mostrar menu de início")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
      "Analógico direito")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN,
      "Executar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
      "Ativar SAMBA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
      "Guardar ficheiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
      "Índice de gravação de estado automático")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
      "Carregar estado automaticamente")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
      "Guardar estado automaticamente")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
      "Guardar estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
      "Miniaturas de gravação de estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
      "Guardar configuração atual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Guardar sobreposições de núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Guardar sobreposições de jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
      "Guardar nova configuração")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_STATE,
      "Guardar estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
      "Guardando")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
      "Verificar pasta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_FILE,
      "Verificar ficheiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
      "<Verificar esta pasta>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
      "Captura de ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
      "Resolução de ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SEARCH,
      "Procurar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SECONDS,
      "segundos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS,
      "Definições")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
      "Definições")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER,
      "Shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
      "Aplicar alterações")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
      "Shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
      "Fita")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
      "Fita (simplificada)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
      "Neve simples")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
      "Neve")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
      "Mostrar definições avançadas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
      "Mostrar ficheiros e pastas ocultas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHUTDOWN,
      "Desligar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
      "Rácio da função de câmera-lenta")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
      "Ordenação de gravação em pastas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
      "Ordenação de gravação de estado em pastas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
      "Ativar SSH")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_CORE,
      "Iniciar núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
      "Iniciar RetroPad remoto")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
      "Iniciar processador de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATE_SLOT,
      "Posição de estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATUS,
      "Estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
      "Comandos stdin")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
      "Núcleos sugeridos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
      "Suspender proteção de ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
      "Ativar sistema BGM")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
      "Sistema/BIOS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
      "Informações do sistema")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
      "Suporte de 7zip")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
      "Suporte de ALSA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
      "Data de compilação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
      "Suporte de Cg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
      "Suporte de Cocoa")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
      "Suporte de interface de comandos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
      "Suporte de CoreText")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
      "Características do processador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
      "Mostrar métrica de DPIs")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
      "Mostrar altura métrica (mm)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
      "Mostrar largura métrica (mm)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
      "Suporte de DirectSound")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
      "Suporte de bibliotecas dinâmica")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
      "Carregamento dinâmico da biblioteca Libretro em tempo de execução")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
      "Suporte de EGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
      "Suporte de OpenGL/Direct3D renderização-para-textura (overlaying com várias passagens)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
      "Suporte de FFmpeg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
      "Suporte de FreeType")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
      "Suporte de STB TrueType")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
      "Identificador da interface visual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
      "Nome da interface visual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
      "Sistema operativo da interface visual")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
      "Versão Git")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
      "Suporte de GLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
      "Suporte de HLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
      "Suporte de JACK")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
      "Suporte de KMS/EGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
      "Suporte de LibretroDB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
      "Suporte de Libusb")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
      "Suporte de Netplay (ponto-a-ponto)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
      "Suporte de interface de comandos de rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
      "Suporte de jogos através de ligações remotas em rede")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
      "Suporte de OpenAL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
      "Suporte de OpenGL ES")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
      "Suporte de OpenGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
      "Suporte de OpenSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
      "Suporte de OpenVG")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
      "Suporte de OSS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
      "Suporte de overlays")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
      "Fonte de energia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
      "Carregada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
      "Carregando")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
      "Descarregando")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
      "Não foi encontrada uma fonte de energia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
      "Suporte de PulseAudio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
      "Suporte de Python (suporte de script para shading)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
      "Suporte de BMP (RBMP)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
      "Nível RetroRating")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
      "Suporte de JPEG (RJPEG)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
      "Suporte de RoarAudio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
      "Suporte de PNG (RPNG)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
      "Suporte de RSound")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
      "Suporte de TGA (RTGA)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
      "Suporte de SDL2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
      "Suporte de imagens SDL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
      "Suporte de SDL1.2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
      "Suporte de Slang")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
      "Suporte de Segmentação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
      "Suporte de Udev")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
      "Suporte de Video4Linux2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
      "Controlador de contexto de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
      "Suporte de Vulkan")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
      "Suporte de Wayland")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
      "Suporte de X11")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
      "Suporte de XAudio2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
      "Suporte de XVideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
      "Suporte de Zlib")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
      "Capturar o ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
      "Tarefas segmentadas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS,
      "Miniaturas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
      "Miniaturas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
      "Atualizador de miniaturas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
      "Capas dos títulos publicados")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
      "Capturas do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
      "Títulos do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
      "Mostrar data e hora")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TRUE,
      "Verdadeiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
      "Ativar assistente de interface do utilizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
      "Assistente de interface do utilizador arranca na inicialização da aplicação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
      "Barra de menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
      "Não foi possível ler o ficheiro comprimido.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
      "Desfazer carregamento de estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
      "Desfazer gravação de estado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNKNOWN,
      "Desconhecido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
      "Atualizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
      "Atualizar recursos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
      "Atualizar perfis de auto-configuração")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
      "Atualizar shaders de Cg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
      "Atualizar batotas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
      "Atualizar ficheiros de informação de núcleos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
      "Atualizar base de dados")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
      "Atualizar shaders de GLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
      "Atualizar Lakka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
      "Atualizar overlays")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
      "Atualizar ficheiros slang dos shaders")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER,
      "Utilizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
      "Interface de utilizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
      "Idioma")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
      "Utilizador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
      "Usar o visualizador de imagens integrado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
      "Usar o reprodutor multimédia integrado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
      "<Utilizar esta pasta>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
      "Permitir rotação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
      "Proporção automática do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
      "Proporção do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
      "Inserção de fotograma preto")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
      "Cortar sobreexploração (recarregar)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
      "Desativar composição do ambiente de trabalho")
#if defined(_3DS)
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
      "Tela Inferior 3DS")
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
      "Controlador de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
      "Filtro de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
      "Filtro de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
      "Filtrar tremulação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
      "Ativar notificações no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
      "Tipo de letra das notificações no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
      "Tamanho das notificações no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
      "Forçar proporção de imagem no ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
      "Desativar, forçadamente, o sRGB FBO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
      "Atraso do fotograma de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
      "Utilizar modo de ecrã completo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
      "Gama de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
      "Utilizar gravação na GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
      "Ativar captura de ecrã na GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
      "Sincronização sólida na GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
      "Sincronização sólida de fotogramas na GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
      "Número máximo de trocas de imagem encadeadas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
      "Notificação no ecrã da posição X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
      "Notificação no ecrã da posição Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
      "Índice do monitor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
      "Utilizar gravação pós-filtro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
      "Taxa de atualização")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
      "Taxa de atualização estimada do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
      "Set Display-Reported Refresh Rate")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
      "Rotação")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
      "Escala em janela")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
      "Escala em inteiros")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
      "Vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
      "Sombreamento de vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
      "Número de passagens de shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
      "Ver parâmetros de shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
      "Carregar pré-definição de shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
      "Guardar pré-definições de shaders como")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
      "Guardar pré-definição de núcleo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
      "Guardar pré-definição de jogo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
      "Ativar contexto de partilha de hardware")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
      "Filtragem bilinear")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
      "Ativar filtro suave")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
      "Intervalo de troca da sincronização vertical (Vsync)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
      "Vídeo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
      "Vídeo segmentado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
      "Deflicker")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "Proporção personalizada da altura do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "Proporção personalizada da largura do ecrã")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
      "Proporção personalizada do ecrã na posição X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
      "Proporção personalizada do ecrã na posição Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
      "Definir largura do ecrã VI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
      "Sincronização vertical (Vsync)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
      "Modo ecrã completo em janela")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
      "Largura da janela")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
      "Altura da janela")
MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
      "Controlador de redes sem fios")
MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
      "Redes sem fios")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
      "Fator alfa do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
      "Cor vermelha no texto do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
      "Cor verde no texto do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
      "Cor azul no texto do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_FONT,
      "Tipo de letra do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
      "Personalizar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
      "Cores planas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
      "Monocromático")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
      "Monocromático invertido")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
      "Sistemático")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
      "Ativo e moderno")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
      "Pixel")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
      "Retroativo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
      "Sistema retro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
      "Arte de pontos")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
      "Cor do tema do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
      "Maça verde")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
      "Escuro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
      "Roxo escuro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
      "Azul elétrico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
      "Dourado")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
      "Vermelho antigo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
      "Azul meia-noite")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
      "Plano")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
      "Submarino")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
      "Vermelho vulcânico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
      "Shader do canal do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR,
      "Fator de escala do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
      "Ativar sombras nos ícones")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
      "Mostrar separador de histórico")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
      "Mostrar separador de importação de conteúdo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
      "Mostrar separador de imagem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
      "Mostrar separador de música")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
      "Mostrar separador de definições")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
      "Mostrar separador de vídeo")
MSG_HASH(MENU_ENUM_LABEL_XMB_LAYOUT,
      "Select a different layout for the XMB interface.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_THEME,
      "Ícone do tema do menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_YES,
      "Sim")
MSG_HASH(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
      "Pré-definição de shader")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
      "Ativar/desativar as conquistas. Para mais informações, visite http://retroachievements.org")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
      "Ativar/desativar conquistas não oficiais e/ou característas beta para fins de testes.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Ativar/desativar gravação de estados, batota, função de retrocedimento no tempo, função de avanço-rápido, pausa e câmera-lenta para todos os jogos.")
MSG_HASH(MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
      "Modificar os controladores utilizados pelo sistema.")
MSG_HASH(MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
      "Modificar as definições de conquistas.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_SETTINGS,
      "Modificar as definições de núcleo.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
      "Modificar as definições de gravação de vídeo.")
MSG_HASH(MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
      "Modificar as definições de apresentação de sobreposição e sobreposição de teclado, além das definições de notificações no ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
      "Modificar as definições das funções de retrocedimento no tempo, avanço-rápido e câmera-lenta.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
      "Modificar as definições de gravação.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
      "Modificar as definições de registo.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
      "Modificar as configurações da interface do utilizador.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_SETTINGS,
      "Modificar as definições de conta, nome de utilizador e idioma.")
MSG_HASH(MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
      "Modificar as definições de privacidade.")
MSG_HASH(MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
      "Modificar o padrão de pastas onde os ficheiros estão localizados.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
      "Modificar as definições de lista de reprodução.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
      "Configurar as definições de servidor e rede.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
      "Procurar por conteúdo e adicionar à base de dados.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
      "Modificar as definições de saída de som.")
MSG_HASH(MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
      "Ativar/desativar Bluetooth.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
      "Guardar as alterações nos ficheiros de configuração ao sair.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
      "Modificar as pré-definições para os ficheiros de configuração.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
      "Gere e cria ficheiros de configuração.")
MSG_HASH(MENU_ENUM_SUBLABEL_CPU_CORES,
      "Número de núcleos que o processador possui.")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_SHOW,
      "Mostrar a taxa de fotogramas atuais por segundo, no ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
      "Configurar teclas de atalho.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "Combinação do botão do comando para mostrar/esconder o menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
      "Alterar as definições de um comando, teclado ou rato.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
      "Configurar as teclas para este utilizador.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
      "Ativar/desativar a entrada no terminal.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY,
      "Juntar ou hospedar uma sessão de netplay.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
      "Procurar e ligar a anfitriões de Netplay na rede local.")
MSG_HASH(MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
      "Mostrar informações do núcleo, rede e sistema.")
MSG_HASH(MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
      "Transferir complementos, componentes e conteúdos para o RetroArch.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
      "Ativar/desativar partilha de pastas na rede.")
MSG_HASH(MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
      "Gerir o nível de serviços do sistema operativo.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
      "Mostrar ficheiros/pastas escondidos no explorador de ficheiros.")
MSG_HASH(MENU_ENUM_SUBLABEL_SSH_ENABLE,
      "Ativar/desativar acesso remoto pela linha de comandos.")
MSG_HASH(MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
      "Prevenir a ativação do protetor de ecrã do sistema.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
      "Definir o tamanho da janela em relação ao tamanho da janela de visualização do núcleo. Como alternativa, você pode definir uma largura de janela e altura abaixo, para um tamanho de janela fixo.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_LANGUAGE,
      "Definir o idioma da interface.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
      "Inserir um fotograma negro entre fotogramas. Útil para utilizadores ecrãs de 120Hz que queiram jogar conteúdos de 60Hz para eliminar os fantasmas gráficos.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
      "Reduzir a latência com o risco de aumentar o engasgamento de vídeo. Adiciona um atraso depois do V-Sync (em ms).")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
      "Definir quantos fotogramas o processador pode executar depois da GPU quando a opção 'Sincronização sólida de GPU' estiver ativa.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
      "Informar o controlador de vídeo sobre a utilização explícita de um modo de carregamento específico.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
      "Seleciona o ecrã a ser utilizado.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
      "A taxa de atualização do ecrã estimada em Hz.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
      "The refresh rate as reported by the display driver.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
      "Alterar as definições da saída de vídeo.")
MSG_HASH(MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
      "Procurar redes sem fio e estabelecer uma ligação.")
MSG_HASH(MENU_ENUM_SUBLABEL_HELP_LIST,
      "Saiba mais sobre a aplicação.")
MSG_HASH(MSG_APPENDED_DISK,
      "Disco anexado")
MSG_HASH(MSG_APPLICATION_DIR,
      "Pasta da aplicação")
MSG_HASH(MSG_APPLYING_CHEAT,
      "Aplicando as alterações de batota.")
MSG_HASH(MSG_APPLYING_SHADER,
      "Aplicando sombreamento")
MSG_HASH(MSG_AUDIO_MUTED,
      "Silenciamento ativo.")
MSG_HASH(MSG_AUDIO_UNMUTED,
      "Removido silêncio.")
MSG_HASH(MSG_AUTOCONFIG_FILE_ERROR_SAVING,
      "Erro ao guardar o ficheiro de auto-configuração.")
MSG_HASH(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
      "O ficheiro de auto-configuração foi guardado com sucesso.")
MSG_HASH(MSG_AUTOSAVE_FAILED,
      "Não foi possível iniciar a gravação automática.")
MSG_HASH(MSG_AUTO_SAVE_STATE_TO,
      "Gravação de estado automática em")
MSG_HASH(MSG_BLOCKING_SRAM_OVERWRITE,
      "Bloqueando a sobrescrição da SRAM")
MSG_HASH(MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
      "Trazendo a interface de comando na porta")
MSG_HASH(MSG_BYTES,
      "bytes")
MSG_HASH(MSG_CANNOT_INFER_NEW_CONFIG_PATH,
      "Não é possível inferir o novo caminho de configuração. Use a hora atual.")
MSG_HASH(MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Modo Hardcore ativado. A gravação de estado e função de retrocedimento no tempo estão desativadas.")
MSG_HASH(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
      "Comparando com números mágicos conhecidos...")
MSG_HASH(MSG_COMPILED_AGAINST_API,
      "Compilado contra API")
MSG_HASH(MSG_CONFIG_DIRECTORY_NOT_SET,
      "Configuração de pasta não definida. Não foi possível guardar a nova configuração.")
MSG_HASH(MSG_CONNECTED_TO,
      "Conectado a")
MSG_HASH(MSG_CONTENT_CRC32S_DIFFER,
      "As somas CRC32 diferem entre si. Não é possível usar jogos diferentes.")
MSG_HASH(MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
      "Carregamento de conteúdo ignorado. A implementação carregará por conta própria.")
MSG_HASH(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
      "O núcleo não suporta gravação de estados.")
MSG_HASH(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
      "Os ficheiros de opções do núcleo foram criados com sucesso.")
MSG_HASH(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
      "Não foi possível encontrar nenhum controlador próximo")
MSG_HASH(MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
      "Não foi possível encontrar uma compatibilidade de sistema.")
MSG_HASH(MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
      "Não foi possível encontrar uma pista de dados válida")
MSG_HASH(MSG_COULD_NOT_OPEN_DATA_TRACK,
      "Não foi possível abrir a pista de dados")
MSG_HASH(MSG_COULD_NOT_READ_CONTENT_FILE,
      "Não foi possível ler o ficheiro de conteúdo")
MSG_HASH(MSG_COULD_NOT_READ_MOVIE_HEADER,
      "Não foi possível ler o cabeçalho do vídeo.")
MSG_HASH(MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
      "Não foi possível ler o estado do vídeo.")
MSG_HASH(MSG_CRC32_CHECKSUM_MISMATCH,
      "A soma de verificação CRC32 do ficheiro de conteúdo não coincide com a soma de verificação guardada no cabeçalho do ficheiro de execução. Existe uma grande probabilidade de acontecer uma dessincronização durante a execução.")
MSG_HASH(MSG_CUSTOM_TIMING_GIVEN,
      "Tempo personalizado fornecido")
MSG_HASH(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
      "Descompressão a decorrer.")
MSG_HASH(MSG_DECOMPRESSION_FAILED,
      "Descompressão falhou.")
MSG_HASH(MSG_DETECTED_VIEWPORT_OF,
      "Detectada janela de exibição de")
MSG_HASH(MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
      "Não encontrou um patch de conteúdo válido.")
MSG_HASH(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
      "Dispositivo desconectado de uma porta válida.")
MSG_HASH(MSG_DISK_CLOSED,
      "Fechado")
MSG_HASH(MSG_DISK_EJECTED,
      "Ejetado")
MSG_HASH(MSG_DOWNLOADING,
      "Transferindo")
MSG_HASH(MSG_DOWNLOAD_FAILED,
      "A transferência falhou")
MSG_HASH(MSG_ERROR,
      "Erro")
MSG_HASH(MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
      "O núcleo do Libretro requer conteúdo, mas o mesmo não foi fornecido.")
MSG_HASH(MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
      "O núcleo do Libretro requer conteúdo especial, mas não foi fornecido nenhum conteúdo.")
MSG_HASH(MSG_ERROR_PARSING_ARGUMENTS,
      "Erro ao analisar os argumentos.")
MSG_HASH(MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
      "Erro ao guardar o ficheiro de opções do núcleo.")
MSG_HASH(MSG_ERROR_SAVING_REMAP_FILE,
      "Erro ao guardar o ficheiro de remapeamento.")
MSG_HASH(MSG_ERROR_SAVING_SHADER_PRESET,
      "Erro ao guardar a pré-definição de sombreamento.")
MSG_HASH(MSG_EXTERNAL_APPLICATION_DIR,
      "Pasta de aplicações externa")
MSG_HASH(MSG_EXTRACTING,
      "Extraindo")
MSG_HASH(MSG_EXTRACTING_FILE,
      "Extraindo o ficheiro")
MSG_HASH(MSG_FAILED_SAVING_CONFIG_TO,
      "Falha ao guardar a configuração em")
MSG_HASH(MSG_FAILED_TO,
      "Falha ao")
MSG_HASH(MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
      "Falha ao aceitar o espectador de entrada")
MSG_HASH(MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
      "Falha ao alocar memória para o conteúdo do patch...")
MSG_HASH(MSG_FAILED_TO_APPLY_SHADER,
      "Falha ao aplicar o shader.")
MSG_HASH(MSG_FAILED_TO_BIND_SOCKET,
      "Falha ao abrir a ligação para o socket.")
MSG_HASH(MSG_FAILED_TO_CREATE_THE_DIRECTORY,
      "Falha ao criar a pasta.")
MSG_HASH(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
      "Falha ao extrair o conteúdo do ficheiro comprimido")
MSG_HASH(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
      "Falha ao obter a alcunha do cliente.")
MSG_HASH(MSG_FAILED_TO_LOAD,
      "Falha ao carregar")
MSG_HASH(MSG_FAILED_TO_LOAD_CONTENT,
      "Falha ao carregar o conteúdo")
MSG_HASH(MSG_FAILED_TO_LOAD_MOVIE_FILE,
      "Falha ao carregar o ficheiro de vídeo")
MSG_HASH(MSG_FAILED_TO_LOAD_OVERLAY,
      "Falha ao carregar a sobreposição.")
MSG_HASH(MSG_FAILED_TO_LOAD_STATE,
      "Falha ao carregar o estado de")
MSG_HASH(MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
      "Falha ao abrir o núcleo do Libretro")
MSG_HASH(MSG_FAILED_TO_PATCH,
      "Falha na atualização")
MSG_HASH(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
      "Falha ao receber o cabeçalho do cliente.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME,
      "Falha ao receber a alcunha.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
      "Falha ao receber a alcunha do anfitrião.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
      "Falha ao receber o tamanho da alcunha do anfitrião.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
      "Falha ao receber os dados da SRAM do anfitrião.")
MSG_HASH(MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
      "Falha ao remover o disco da área de notificações.")
MSG_HASH(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
      "Falha ao remover o ficheiro temporário")
MSG_HASH(MSG_FAILED_TO_SAVE_SRAM,
      "Falha ao guardar a SRAM")
MSG_HASH(MSG_FAILED_TO_SAVE_STATE_TO,
      "Falha ao guardar o estado para")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME,
      "Falha ao enviar a alcunha.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_SIZE,
      "Falha ao enviar o tamanho da alcunha.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
      "Falha ao enviar a alcunha para o cliente.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
      "Falha ao enviar a alcunha para o anfitrião.")
MSG_HASH(MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
      "Falha ao enviar os dados SRAM para o cliente.")
MSG_HASH(MSG_FAILED_TO_START_AUDIO_DRIVER,
      "Falha ao iniciar o controlador de som. Continuará sem som.")
MSG_HASH(MSG_FAILED_TO_START_MOVIE_RECORD,
      "Falha ao iniciar a gravação do vídeo.")
MSG_HASH(MSG_FAILED_TO_START_RECORDING,
      "Falha ao iniciar a gravação.")
MSG_HASH(MSG_FAILED_TO_TAKE_SCREENSHOT,
      "Falha ao obter uma captura de ecrã.")
MSG_HASH(MSG_FAILED_TO_UNDO_LOAD_STATE,
      "Falha ao desfazer o carregamento de estado.")
MSG_HASH(MSG_FAILED_TO_UNDO_SAVE_STATE,
      "Falha ao desfazer a gravação de estado.")
MSG_HASH(MSG_FAILED_TO_UNMUTE_AUDIO,
      "Falha ao remover o silêncio.")
MSG_HASH(MSG_FATAL_ERROR_RECEIVED_IN,
      "Erro fatal recebido em")
MSG_HASH(MSG_FILE_NOT_FOUND,
      "Ficheiro não encontrado")
MSG_HASH(MSG_FOUND_AUTO_SAVESTATE_IN,
      "Gravação de estado automática encontrado em")
MSG_HASH(MSG_FOUND_DISK_LABEL,
      "Rótulo de disco encontrado")
MSG_HASH(MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
      "Encontrada a primeira pista de dados no ficheiro")
MSG_HASH(MSG_FOUND_LAST_STATE_SLOT,
      "Encontrada a última posição de estado")
MSG_HASH(MSG_FOUND_SHADER,
      "Sombreamento encontrado")
MSG_HASH(MSG_FRAMES,
      "Fotogramas")
MSG_HASH(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
      "Opções por jogo: núcleos específicos de jogo encontrados em")
MSG_HASH(MSG_GOT_INVALID_DISK_INDEX,
      "Há um índice de disco inválido")
MSG_HASH(MSG_GRAB_MOUSE_STATE,
      "Capturar estado do rato")
MSG_HASH(MSG_GAME_FOCUS_ON,
      "Foco de jogo ligado")
MSG_HASH(MSG_GAME_FOCUS_OFF,
      "Foco de jogo desligado")
MSG_HASH(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
      "O núcleo do Libretro é renderizadi por hardware. Deve ser usada a gravação de pós-sombreamento.")
MSG_HASH(MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
      "A soma de verificação inflada não corresponde ao CRC32.")
MSG_HASH(MSG_INPUT_CHEAT,
      "Introduzir batota")
MSG_HASH(MSG_INPUT_CHEAT_FILENAME,
      "Nome do ficheiro de batota")
MSG_HASH(MSG_INPUT_PRESET_FILENAME,
      "Nome de ficheiro pré-definido")
MSG_HASH(MSG_INPUT_RENAME_ENTRY,
      "Renomear título")
MSG_HASH(MSG_INTERFACE,
      "Interface")
MSG_HASH(MSG_INTERNAL_STORAGE,
      "Armazenamento interno")
MSG_HASH(MSG_REMOVABLE_STORAGE,
      "Armazenamento removível")
MSG_HASH(MSG_INVALID_NICKNAME_SIZE,
      "O tamanho da alcunha é inválido.")
MSG_HASH(MSG_IN_BYTES,
      "em bytes")
MSG_HASH(MSG_IN_GIGABYTES,
      "em gigabytes")
MSG_HASH(MSG_IN_MEGABYTES,
      "em megabytes")
MSG_HASH(MSG_LIBRETRO_ABI_BREAK,
      "é compilado contra uma versão diferente do Libretro com esta implementação do Libretro.")
MSG_HASH(MSG_LIBRETRO_FRONTEND,
      "Frontend para Libretro")
MSG_HASH(MSG_LOADED_STATE_FROM_SLOT,
      "Estado carregado a partir da posição #%d.")
MSG_HASH(MSG_LOADED_STATE_FROM_SLOT_AUTO,
      "Estado carregado a partir da posição #-1 (automático).")
MSG_HASH(MSG_LOADING,
      "Carregando")
MSG_HASH(MSG_FIRMWARE,
      "Um ou mais ficheiros de firmware estão perdidos")
MSG_HASH(MSG_LOADING_CONTENT_FILE,
      "Carregando ficheiro de conteúdo")
MSG_HASH(MSG_LOADING_HISTORY_FILE,
      "Carregando ficheiro de histórico")
MSG_HASH(MSG_LOADING_STATE,
      "Carregando estado")
MSG_HASH(MSG_MEMORY,
      "Memória")
MSG_HASH(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
      "O ficheiro de vídeo não é um ficheiro BSV1 válido.")
MSG_HASH(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
      "O formato do ficheiro parece ter uma versão de serializador diferente. A tarefa poderá falhar.")
MSG_HASH(MSG_MOVIE_PLAYBACK_ENDED,
      "Lista de reprodução de vídeos terminou.")
MSG_HASH(MSG_MOVIE_RECORD_STOPPED,
      "Parando a gravação de vídeo.")
MSG_HASH(MSG_NETPLAY_FAILED,
      "Falha ao iniciar o Netplay.")
MSG_HASH(MSG_NO_CONTENT_STARTING_DUMMY_CORE,
      "Sem conteúdo. Iniciando um núcleo vazio.")
MSG_HASH(MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
      "Não existe um estado de gravação para ser substitudo.")
MSG_HASH(MSG_NO_STATE_HAS_BEEN_LOADED_YET,
      "Não existem estados disponíveis para carregamento.")
MSG_HASH(MSG_OVERRIDES_ERROR_SAVING,
      "Erro ao guardar as substituições.")
MSG_HASH(MSG_OVERRIDES_SAVED_SUCCESSFULLY,
      "Substituições gravadas com sucesso.")
MSG_HASH(MSG_PAUSED,
      "Pausado.")
MSG_HASH(MSG_PROGRAM,
      "RetroArch")
MSG_HASH(MSG_READING_FIRST_DATA_TRACK,
      "Lendo a primeira pista de dados...")
MSG_HASH(MSG_RECEIVED,
      "Recebido")
MSG_HASH(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
      "A gravação terminou devido ao redimensionamento.")
MSG_HASH(MSG_RECORDING_TO,
      "Gravando em")
MSG_HASH(MSG_REDIRECTING_CHEATFILE_TO,
      "Redirecionando o ficheiro de batota em")
MSG_HASH(MSG_REDIRECTING_SAVEFILE_TO,
      "Redirecionando o ficheiro de gravação em")
MSG_HASH(MSG_REDIRECTING_SAVESTATE_TO,
      "Redirecionando a gravação do estado em")
MSG_HASH(MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
      "Remapeamento do ficheiro guardado com sucesso.")
MSG_HASH(MSG_REMOVED_DISK_FROM_TRAY,
      "Disco removido da área de notificação.")
MSG_HASH(MSG_REMOVING_TEMPORARY_CONTENT_FILE,
      "Removendo conteúdo temporário do ficheiro")
MSG_HASH(MSG_RESET,
      "Reiniciar (reset)")
MSG_HASH(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
      "Reiniciando a gravação devido ao reinício do controlador.")
MSG_HASH(MSG_RESTORED_OLD_SAVE_STATE,
      "Gravação do estado antigo restaurada.")
MSG_HASH(MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
      "Sombreamento: restaurando padrões de pré-definição de sombreamento em")
MSG_HASH(MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
      "Revertendo a gravação do ficheiro em")
MSG_HASH(MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
      "Revertendo a gravação de estado em")
MSG_HASH(MSG_REWINDING,
      "Retrocedendo no tempo.")
MSG_HASH(MSG_REWIND_INIT,
      "Inicializando o buffer da função de retrocedimento no tempo com tamanho")
MSG_HASH(MSG_REWIND_INIT_FAILED,
      "Falha ao incializar o buffer da função de retrocedimento do tempo. A função de retrocedimento no tempo será desativada.")
MSG_HASH(MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
      "Esta implementação utiliza som encadeado. Não é possível usar o recurso de retrocedimento do tempo.")
MSG_HASH(MSG_REWIND_REACHED_END,
      "Alcançado o final do buffer do recurso de retrocedimento do tempo.")
MSG_HASH(MSG_SAVED_NEW_CONFIG_TO,
      "Guardar nova configuração em")
MSG_HASH(MSG_SAVED_STATE_TO_SLOT,
      "Estado guardado na posição #%d.")
MSG_HASH(MSG_SAVED_STATE_TO_SLOT_AUTO,
      "Estado guardado na posição #-1 (automático).")
MSG_HASH(MSG_SAVED_SUCCESSFULLY_TO,
      "Guardado com sucesso em")
MSG_HASH(MSG_SAVING_RAM_TYPE,
      "Guarando o tipo de RAM")
MSG_HASH(MSG_SAVING_STATE,
      "Guardando o estado")
MSG_HASH(MSG_SCANNING,
      "Verificando")
MSG_HASH(MSG_SCANNING_OF_DIRECTORY_FINISHED,
      "Verificação da pasta terminada")
MSG_HASH(MSG_SENDING_COMMAND,
      "Enviando comando")
MSG_HASH(MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
      "Vários ajustes foram definidos explicitamente. Ignorando todos...")
MSG_HASH(MSG_SHADER,
      "Sombreamento")
MSG_HASH(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
      "Pré-definição de sombreamento guardada com sucesso.")
MSG_HASH(MSG_SKIPPING_SRAM_LOAD,
      "Ignorando o carregamento de SRAM.")
MSG_HASH(MSG_SLOW_MOTION,
      "Câmera lenta.")
MSG_HASH(MSG_FAST_FORWARD,
      "Avanço-rápido.")
MSG_HASH(MSG_SLOW_MOTION_REWIND,
      "Função de retrocedimento do tempo em câmera lenta.")
MSG_HASH(MSG_SRAM_WILL_NOT_BE_SAVED,
      "SRAM não será guardada.")
MSG_HASH(MSG_STARTING_MOVIE_PLAYBACK,
      "Iniciando reprodução de filme.")
MSG_HASH(MSG_STARTING_MOVIE_RECORD_TO,
      "Iniciando a gravação do filme para")
MSG_HASH(MSG_STATE_SIZE,
      "Tamanho do estado")
MSG_HASH(MSG_STATE_SLOT,
      "Posição de estado")
MSG_HASH(MSG_TAKING_SCREENSHOT,
      "Obtendo captura de ecrã")
MSG_HASH(MSG_TO,
      "para")
MSG_HASH(MSG_UNDID_LOAD_STATE,
      "Desfez o carregamento do estado.")
MSG_HASH(MSG_UNDOING_SAVE_STATE,
      "Desfazendo a gravação do estado")
MSG_HASH(MSG_UNKNOWN,
      "Desconhecido")
MSG_HASH(MSG_UNPAUSED,
      "Sem interrupções.")
MSG_HASH(MSG_UNRECOGNIZED_COMMAND,
      "Comando não reconhecido")
MSG_HASH(MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
      "Utilizando o nome do núcleo para uma nova configuração.")
MSG_HASH(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
      "Utilizando o núcleo fictício do Libretro. Saltando a gravação.")
MSG_HASH(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
      "Ligue o dispositivo a uma porta válida.")
MSG_HASH(MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
      "Desligando o dispositivo da porta...")
MSG_HASH(MSG_VALUE_REBOOTING,
      "Reiniciando...")
MSG_HASH(MSG_VALUE_SHUTTING_DOWN,
      "Encerrando...")
MSG_HASH(MSG_VERSION_OF_LIBRETRO_API,
      "Versão da API Libretro")
MSG_HASH(MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
      "Falha no cálculo do tamanho da janela de visualização! Continuarão a serem utilizados dados em bruto. Provavelmente, irão surgir erros ...")
MSG_HASH(MSG_VIRTUAL_DISK_TRAY,
      "Ícone do disco virtual na área de notificações.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
      "Latência de som desejada em milisegundos. Este parâmetro pode não ser honrado, caso o controlador de som não possa atingir a latência dada.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MUTE,
      "Silenciar som/remover silêncio.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
      "Ajuda a suavizar as imperfeições no sincronismo entre som e vídeo. Caso se encontre desativada, esta opção fará com que a sincronização adequada se torne quase impossível de se concretizar."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
      "Permitir ou não o acesso à câmera pelos núcleos."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
      "Permitir ou não o acesso ao serviço de localização aos núcleos."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
      "Número máximo de utilizadores suportados pelo RetroArch."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
      "Comportamento de procura de periféricos de entrada no RetroArch. Se o valor for 'Cedo' ou 'Tarde', poder obter menos latência, dependendo da sua configuração."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
      "Permitir o controlo do menu por qualquer utilizador. Quando desativada, esta opção faz com que apenas o Utilizador 1 possa controlar o menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
      "Volume de som (em dB). 0 dB representa o volume normal, sem aplicação de ganho."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_SYNC,
      "Sincronizar o som. Recomendado.")
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Quantidade de segundos a aguardar até que seja feita uma nova associação."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Indica o período em que os botões de turbo alternam entre si. A unidade utilizada são fotogramas."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Indica o tempo necessário para a ativação do botão turbo. A unidade utilizada são fotogramas."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Sincroniza o vídeo de saída da placa gráfica com a taxa de atualização do ecr. Recomendado."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Permite que os núcleos definam a rotação. Quando desativada, esta opção faz com que as requisições de rotação sejam ignoradas. Útil para configurações onde a imagem apresentada no ecrã entra em rotação, de forma manual."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Alguns núcleos podem ter uma funcionalidade de encerramento. Caso esteja ativa, esta definição irá impedir que o núcleo termine o RetroArch. Em vez disso, será carregado um núcleo fictício."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Verifica se todo o firmware necessário está presentes antes de tentar carregar conteúdo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Taxa de atualização vertical do ecrã. Utilizada no cálculo da taxa de saída de som adequada. Obs: Esta definição será ignorada se a opção 'Vídeo segmentado' estiver ativa."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Ativar som."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Alteração máxima na taxa de entrada de som. Se for ativada, esta opção irá causar grandes mudanças na sincronia, resultando na perda de precisão da tonalidade do som (ex: executar um núcleo PAL e apresentado em NTSC)."
   )
MSG_HASH(
   MSG_FAILED,
   "Falha"
   )
MSG_HASH(
   MSG_SUCCEEDED,
   "Sucesso"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "Não configurado"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "Não configurado. Será utilizado o auxiliar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Lista de cursores da base de dados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Filtro de dase de dados : Estúdio de desenvolvimento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Filtro de base de dados : Editor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Desativado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Ativado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Caminho do histórico de conteúdo"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
      "Base de dados - Filtro : Origem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
      "Base de dados - Filtro : Franquia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
      "Base de dados - Filtro : Classificação ESRB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
      "Base de dados - Filtro : Classificação ELSPA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
      "Base de dados - Filtro : Classificação PEGI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
      "Base de dados - Filtro : Classificação CERO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
      "Base de dados - Filtro : Classificação BBFC")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
      "Base de dados - Filtro : Máximo de utilizadores")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
      "Base de dados - Filtro : Data de lançamento por mês")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
      "Base de dados - Filtro : Data de lançamento por ano")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
      "Base de dados - Filtro : Edição da revista Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
      "Base de dados - Filtro : Classificação da revista Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
      "Informações da base de dados")
MSG_HASH(MSG_WIFI_SCAN_COMPLETE,
      "Procura de redes sem fios completa.")
MSG_HASH(MSG_SCANNING_WIRELESS_NETWORKS,
      "Procurando por redes sem fios...")
MSG_HASH(MSG_NETPLAY_LAN_SCAN_COMPLETE,
      "Procura de sessões Netplay completa.")
MSG_HASH(MSG_NETPLAY_LAN_SCANNING,
      "Procurando por anfitriões de Netplay...")
MSG_HASH(MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
      "Pausar o jogo quando a janela do RetroArch não estiver ativa.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
      "Ativar ou desativar composição (Apenas em Windows).")
MSG_HASH(MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
      "Ativar ou desativar a lista de reprodução recente para jogos, imagens, música e vídeos.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
      "Limite o número de acesso e lista de reprodução para jogos, imagens, música e vídeos.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
      "Controlos de menu unificados")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
      "Usar as mesmas teclas para o menu e para o jogo. Aplica-se ao teclado.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
      "Mostrar mensagens no ecrã.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
      "Permitir utilizador remoto %d")
MSG_HASH(MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
      "Mostrado estado da bateria")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SELECT_FILE,
      "Selecionar ficheiro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
      "Selecionar da Lista de Reprodução")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FILTER,
      "Filtro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCALE,
      "Escala")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
      "O Netplay irá iniciar quando o conteúdo for carregado.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
      "Não foi possível encontrar um núcleo adequado ou um ficheiro de conteúdo. Por favor, carregue manualmente.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
      "Navegar pelo URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_URL,
      "Caminho do URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_START,
      "Iniciar"
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
      "Bokeh")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
      "Atualizar")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
      "Alcunha: %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
      "Alcunha (lan): %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
      "Conteúdo compatível encontrado")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
      "Corta alguns pixeis em torno das bordas da imagem, habitualmente deixada em branco por estúdios de desenvolvimento e que, por vezes, contêm pixeis de lixo.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
      "Adiciona um leve borrão à imagem para tirar a borda de pixeis rígidos. Esta opção tem pouco impacto no desempenho.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FILTER,
      "Aplica um filtro de vídeo alimentado por CPU. Obs: Pode ter um custo elevado no desempenho. Alguns filtros de vídeo poderão ser apenas utilizados em núcleos que utilizam processadores de 16/32 bits.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
      "Introduza o nome de utilizador da sua conta Retro Achievements.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
      "Introduza a palavra-passe da sua conta Retro Achievements.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
      "Introduza aqui o seu nome de utilizador. Esta informação será usada, principalmente, em sessões do Netplay.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
      "Capturar a imagem depois da aplicação dos filtros (excluindo os sombreamentos). O vídeo gravado ficará tão elegante quanto o que for exibido no ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_LIST,
      "Selecione o núcleo que deseja utilizar.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
      "Selecione o conteúdo que deseja executar.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
      "Mostrar interfaces de rede e endereços de IP associados.")
MSG_HASH(MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
      "Mostrar informações específicas do dispositivo.")
MSG_HASH(MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
      "Sair do programa.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
      "Define a largura personalizada para a janela de exibição. Se o valor for 0, a janela irá ficar o mais larga possível.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
      "Define a altura personalizada para a janela de exibição. Se o valor for 0, a janela irá ficar o mais alta possível.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
      "Especifique a posição do eixo X personalizada para o texto do ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
      "Especifique a posição do eixo Y personalizada para o texto do ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
      "Especifique o tamanho da fonte em pontos.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
      "Ocultar a sobreposição quando o menu estiver aberto e mostrar novamente ao encerrar o mesmo.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
      "O conteúdo verificado aparecerá aqui."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
      "Apenas escalas de vídeo em etapas inteiras. O tamanho da base depende da geometria relatada pelo sistema e das proporções. Se a função 'Forçar proporções' não estiver definido, X / Y será escalado como inteiro, de forma independente."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
      "Capturas de ecrã irão mostrar as camadas de shader geradas pelo GPU, caso estejam disponíveis."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
      "Força uma certa rotação do ecrã. A rotação é adicionada às rotações que o núcleo define."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
      "Desativar o suporte de sRGB FBO. Alguns controladores Intel OpenGL para o Windows possuem problemas de vídeo em sRGB FBO, se o mesmo estiver ativo. Ao ativar esta opção, poderá resolver esse problema."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
      "Executar em ecrã completo. Esta definição poderá ser alterada a qualquer momento."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
      "Em modo de ecrã completo, será utilizada uma janela para apresentar o contedo."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
      "Grava a saída da camada de shader gerada pela GPU, se estiver disponível."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
      "Quando for feita uma gravação de estado, o índice de gravação do estado é aumentado automaticamente antes de ser guardado. Quando for carregado conteúdo, o índice será alterado para o índice mais alto existente."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
      "Impedir a sobreposição da Save RAM durante o carregamento de estados de gravação. Pode provocar perturbações nos jogos."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
      "A taxa máxima na qual o conteúdo será executado quando for utilizada a função de avanço-rápido (ex: 5.0x para conteúdos em 60fps, existir um limite de 300 fotogramas p/ segundo). Se for definida como 0.0x, a taxa de avanço-rápido é ilimitada (sem limite de fotogramas p/ segundo)."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
      "Quando está em câmera-lenta, o conteúdo será diminuído pelo fator especificado/definido."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_REWIND_ENABLE,
      "Ativando o retrocedimento no tempo. Isso irá afetar o desemprenho quando utilizado."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
      "Ao retroceder o tempo durante um número definido de fotogramas, você poderá retroceder vários fotogramas de cada vez, aumentando a velocidade da função."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
      "Define o nível de registo para os núcleos. Se um nível de registo enviado por um núcleo se encontrar abaixo deste valor, o mesmo é ignorado."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
      "Ativa os contadores de desempenho para o RetroArch (e núcleo)."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
      "Cria automaticamente uma gravação de estado no final da execução do RetroArch. O RetroArch irá carregar automaticamente este estado se a função 'Carregar estado automaticamente' estiver ativada."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
      "Carregar automaticamente o último estado automático guardado no arranque do RetroArch."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
      "Mostrar miniaturas dos estados guardados no menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
      "Guarda automaticamente o Save RAM não volátil num intervalo regular. Esta opção encontra-se desativada por omissão. O intervalo é medido em segundos. Um valor de 0 desativa a gravação automática."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
      "Quando ativada, esta definição faz com que as associações de entrada sejam substituídas pelas associações remapeadas do núcleo selecionado."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
      "Ativa a deteção automática de perifricos de entrada. Isto fará com que se configure automaticamente os comandos do tipo 'Plug and Play'."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
      "Muda os botões para OK/Cancelar. Esta opção encontra-se desativada na orientação de botão em japonês e ativado na orientação ocidental."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
      "Quando desativado, o conteúdo continuará a ser executado em segundo plano quando se apresenta ou se esconde o menu do RetroArch."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
      "Controlador de vídeo."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
      "Controlador de som."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_DRIVER,
      "Controlador de entrada. Dependendo do controlador de vídeo, pode forçá-lo a um controlador de entrada diferente."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
      "Controlador de comandos."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
      "Controlador de reamostragem de som."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
      "Controlador de câmera."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
      "Controlador de localização."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_DRIVER,
      "Controlador de menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RECORD_DRIVER,
      "Controlador de gravação."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_WIFI_DRIVER,
      "Controlador de redes sem fios."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "Filtra os ficheiros apresentados no explorador de ficheiros pelas extenções suportadas."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
      "Seleciona uma imagem para definir como fundo do menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
      "Carrega, de forma dinâmica, uma nova imagem de fundo, dependendo do contexto."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
      "Subtitui o dispositivo de som pré-definido pelo controlador de som. Isto varia de acordo com o controlador."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
      "Plugin de som DSP que processa o som antes de ser enviado para o controlador."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
      "Taxa de amostragem da saída de som."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
      "Opacidade de todos os elementos da camada sobreposta."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
      "Escala de todos os elementos da camada sobreposta."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
      "Ativa a camada sobreposta."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
      "Seleciona uma camada de sobreposição do explorador de ficheiros."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
      "Endereço do anfitrião para ligação."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
      "Porta do endereço de IP do anfitrião. Pode ser uma porta TCP ou UDP."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
      "Palavra-passe para ligação ao anfitrião de Netplay. Utilizada apenas em modo hospedeiro."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
      "Anunciar os jogos de Netplay publicamente. Se não estiver ativa esta opção, os clientes deverão ligar-se manualmente, em vez de utilizar a receção pública."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
      "Palavra-passe de ligação ao anfitrião de netplay, apenas com privilégios de espectador. Usada apenas no modo anfitrião."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
      "Executa o Netplay num modo onde não existe gravação de estados. Quando ativada, esta opção faz com que uma rede muito rápida seja necessária, mas nenhuma função Rebobinar poderá ser executada, tornando a sessão de Neyplay mais estável."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
      "Frequência em fotogramas na qual o Netplay irá verificar se o anfitrião e o cliente estão sincronizados."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
      "Durante as sessões como anfitrião, tentar escutar por ligações de Internet pública, recorrendo ao UPnP ou tecnologias semelhantes para ultrapassar as redes locais."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
      "Ativar interface de comandos pelo stdin."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
      "Ativar controlo do rato no menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_POINTER_ENABLE,
      "Ativar controlo de toque no menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_THUMBNAILS,
      "Tipo de miniatura a ser mostrada."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
      "Mostrar data e/ou hora atuais no menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
      "Mostrar o nível de bateria atual no menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
      "Envolver o início e/ou o final, caso o limite da lista seja alcançado horizontal ou verticalmente."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
      "Ativar o Netplay em modo anfitrião (servidor)."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
      "Ativar o Netplay em modo de cliente.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
      "Terminar uma ligação de Netplay ativa.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
      "Verificar uma pasta por ficheiros compatíveis.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_FILE,
      "Verificar um ficheiro compatível.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
      "Usa um intervalo de troca personalizado para Vsync. Utilize isto para reduzir efetivamente a taxa de atualização do monitor."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
      "Ordenar ficheiros guardados pelo nome da pasta do núcleo utilizado."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
      "Ordenar gravação de estados pelo nome da pasta do núcleo utilizado."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
      "URL do diretório de atualização de núcleos do Libretro no buildbot.")
MSG_HASH(MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
      "URL do diretório de atualização de recursos do Libretro no buildbot.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "Após a transferência, extrair automaticamente o conteúdo dos ficheiros transferidos."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
      "Verificar por novas ROMs.")
MSG_HASH(MENU_ENUM_SUBLABEL_DELETE_ENTRY,
      "Remover esta entrada da lista de reprodução.")
MSG_HASH(MENU_ENUM_SUBLABEL_INFORMATION,
      "Ver mais informações sobre o conteúdo.")
MSG_HASH(MENU_ENUM_SUBLABEL_RUN,
      "Executar o conteúdo.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
      "Alterar definições do explorador de ficheiros.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
      "Ativar teclas personalizadas no arranque, por defeito."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
      "Ativar configuração personalizada no arranque, por defeito."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
      "Ativar as opções específicas do núcleo no arranque.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_ENABLE,
      "Mostrar o nome do núcleo atual no menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
      "Ver a base de dados.")
MSG_HASH(MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
      "Ver pesquisas anteriores.")
MSG_HASH(MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
      "Capturar uma imagem do ecrã.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
      "Fechar o conteúdo atual. As alterações que não se encontram guardadas serão perdidas."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_STATE,
      "Carregar um estado guardado na posição selecionada atualmente.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_STATE,
      "Guardar um estado na posição selecionada atualmente.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESUME,
      "Retomar a execução do conteúdo atual e sair do 'Menu rápido'.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESUME_CONTENT,
      "Retomar a execução do conteúdo atual e deixar o 'Menu rápido'.")
MSG_HASH(MENU_ENUM_SUBLABEL_STATE_SLOT,
      "Altera a posição de estado selecionada.")
MSG_HASH(MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
      "Se o estado for carregado, o conteúdo voltará ao estado anterior ao carregamento.")
MSG_HASH(MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
      "Se o estado foi sobrescrito, ele voltará ao estado de gravação anterior.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
      "Serviço 'Retro Achievements'. Para mais informações, visite http://retroachievements.org (em inglês)"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
      "Gerir contas."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
      "Gerir as definições da função 'Rebobinar'.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESTART_CONTENT,
      "Reinicia o conteúdo.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Guarda um ficheiro de configuração de substituição que será aplicado a todo o conteúdo carregado com esse núcleo. Terá precedência sobre a configuração principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Guarda um ficheiro de configuração de substituição que será aplicado apenas no conteúdo atual. Terá precedência sobre a configuração principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
      "Configurar códigos de batota.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
      "Configurar sombreamento para aumentar a definição da imagem.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
      "Altera as teclas para a execução de conteúdo atual.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_OPTIONS,
      "Altera as opções para a execução de conteúdo atual.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
      "Mostrar as definições avançadas (pré-definição: desativado).")
MSG_HASH(MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
      "Executar tarefas numa thread independente.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
      "Permitir que o utilizador remova entradas das listas de reprodução.")
MSG_HASH(MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
      "Define a pasta de sistema. Os núcleos podem verificar esta pasta para o carregamento de ficheiros BIOS, configurações específicas de sistema, etc.")
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
      "Define a pasta inicial para o explorador de ficheiros.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CONTENT_DIR,
      "Habitualmente, definido pelos estúdios de desenvolvimento que agrupam aplicações libretro/RetroArch, tendo como destino os recursos."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
      "Pasta de armazenamento de imagens de fundo que podem ser carregadas dinamicamente pelo nome, dependendo do contexto.")
MSG_HASH(MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
      "Miniaturas auxiliares (capas e imagens diversas) são armazenadas aqui."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
      "Define o pasta inicial para o menu de configuração do explorador de ficheiros.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
      "O número de fotogramas de latência de entrada para o Netplay utilizar para ocultar a latência da rede. Reduz a instabilidade e faz com que a funcionalidade de Netplay seja menos exigente para o processador, com um custo perceptível de tempo de atraso.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
      "O intervalo de latência entre fotogramas pode ser usado para ocultar a latência da rede. Reduz a instabilidade e faz com que a funcionalidade de Netplay seja menos exigente para o processador, com um custo de latência imprevisível.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_CYCLE_TRAY_STATUS,
      "Ciclo do disco atual. Se o disco estiver inserido, o mesmo será ejetado. Caso contrário, ele será inserido. ")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_INDEX,
      "Alterar o índice do disco.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_OPTIONS,
      "Gestão de imagens de disco.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
      "Selecione uma imagem de disco para inserir.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
      "Certifique-se de que a taxa de fotogramas atingida enquanto estiver dentro do menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_LAYOUT,
      "Select a different layout for the XMB interface.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_THEME,
      "Selecionar um tema diferente para este ícone. As alterações terão efeito após o reinício do programa.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
      "Ativar as sombras em todos os ícones. Isso terá um impacto de desempenho menor.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
      "Selecionar um tema com um gradiente de fundo diferente.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
      "Modificar a opacidade da imagem de fundo.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
      "Selecionar um tema com um gradiente de fundo diferente.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
      "Selecionar um efeito de fundo animado. Isto poderá aumentar significativamente a utilização do processador, de acordo com o efeito. Se o desempenho não for satisfatório, desative este efeito ou altere para um mais simples.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_FONT,
      "Selecionar uma fonte principal diferente para ser usada pelo menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
      "Mostrar o separador de imagem dentro do menu principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
      "Mostrar o separador de música dentro do menu principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
      "Mostrar o separador de vídeo dentro do menu principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
      "Mostrar o separador de definições dentro do menu principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
      "Mostrar o separador de histórico recente dentro do menu principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
      "Mostrar o separador de importação de conteúdo no menu principal.")
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
      "Mostrar o ecrã inicial no menu. Esta opção é desativada automaticamente após o primeiro arranque do RetroArch.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
      "Modificar a opacidade do gráfico do cabeçalho.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
      "Modificar a opacidade do gráfico do rodapé.")
MSG_HASH(MENU_ENUM_SUBLABEL_DPI_OVERRIDE_ENABLE,
      "O menu normalmente irá redimensionar-se, de forma dinâmica. Se você desejar definir uma dimensão em vez disso, ative esta opção.")
MSG_HASH(MENU_ENUM_SUBLABEL_DPI_OVERRIDE_VALUE,
      "Definir o tamanho do dimensionamento personalizado aqui. OBS: Você deve ativar a opção 'Sobreposição de DPI' para que este dimensionamento surta efeito.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
      "Guardar todos os ficheiros transferidos nesta pasta.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
      "Guardar todos as teclas remapeadas nesta pasta.")
MSG_HASH(MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
      "Pasta onde o programa procura por conteúdo/núcleo.")
MSG_HASH(MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
      "Os ficheiros de informação da aplicação/núcleo são armazenados aqui.")
MSG_HASH(MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
      "Se um comando estiver conectado, o mesmo será configurado automaticamente se o ficheiro de configuração correspondente estiver presente nesta pasta.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
      "Guardar todas as listas de reprodução nesta pasta.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
      "Se for definido, o conteúdo que for extraído, de forma temporária, será extraído para esta pasta."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
      "As pesquisas guardadas são armazenadas nesta pasta.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
      "As bases de dados são armazenadas nesta pasta."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
      "Esta localização é verificada por padrão quando a aplicação tenta procurar por recursos carregáveis, etc."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
      "Guardar todos os ficheiros nesta pasta. Se não for definida, será feita uma tentativa de gravação na pasta de trabalho do ficheiro.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
      "Guardar todas as gravações de estado nesta pasta. Se não for definida, será feita uma tentativa de gravação na pasta de trabalho do ficheiro.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
      "Pasta de armazenamento das capturas de ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
      "Define um pasta para armazenamento de sobreposições, para acesso fácil.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
      "Os ficheiros de batota serão armazenados aqui."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
      "Pasta onde serão armazenados os ficheiros de filtro de som DSP."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
      "Pasta onde serão armazenados os ficheiros de filtro de vídeo baseado em CPU."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
      "Define uma pasta onde os ficheiros de sombreamento de vídeo baseado em GPU serão armazenados, para fácil acesso.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
      "As gravações serão armazenadas nesta pasta.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
      "As configurações de gravação serão armazenadas aqui.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
      "Selecione uma fonte diferente para as notificações no ecrã.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
      "As alterações das configurações do shader têm efeito imediato. Utilize isto caso tenha alterado a quantidade de passagens de sombreamento, filtros, escala FBO, etc.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
      "Aumentar ou diminuir a quantidade de passagens do canal do shader. Você pode adicionar um sombreamento separado para cada passagem do canal e configurar a sua escala e filtro."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
      "Carregar uma pré-definição de sombreamento. O canal do shader será definido automaticamente.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
      "Guardar as definições de shader atuais como novas pré-definições de sombreamento.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
      "Guardar as definições de shader atuais como definições padrão para esta aplicação/núcleo.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
      "Guardar as definições de shader atuais como definições padrão para o conteúdo.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
      "Modifica o conteúdo do shader atual diretamente. As alterações não serão guardadas no ficheiro pré-definido.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
      "Modifica a pré-definição de shader próprio que é utilizado no menu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
      "Aumentar ou diminuir a quantidade de batota."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
      "As alterações provocadas pela batota terão efeito imediato.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
      "Carregar um ficheiro de batota."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
      "Guardar as batotas atuais como um ficheiro de gravação de jogo."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
      "Aceder rapidamente a todas as definições no jogo relevantes.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_INFORMATION,
      "Visualizar informações pertinentes para a aplicação/núcleo.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "Personalizar a altura da janela de exibição que é usada se a opção 'Proporção de ecrã' estiver definida como 'Personalizado'.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "Personalizar a largura da janela de exibição que é usada se a opção 'Proporção de ecrã' estiver definida como 'Personalizado'.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
      "Personalizar o deslocamento da janela de exibição usada para definir o eixo-X da janela de exibição. Esta definição será ignorada caso a opção 'Escala Inteira' esteja ativa. Então, ele será centrado automaticamente.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
      "Personalizar o deslocamento da janela de exibição usada para definir o eixo-Y da janela de exibição. Esta definição será ignorada caso a opção 'Escala Inteira' esteja ativa. Então, ele será centrado automaticamente.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
      "Utilizar servidor de retransmissão")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
      "Retransmite as ligações de Netplay através de um servidor homem-no-meio. Útil se o anfitrião se encontrar atrás de uma Firewall ou tiver problemas com os protocolos NAT/UPnP.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
      "Adicionar ao misturador")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
      "Add to mixer and play")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
      "Adicionar ao misturador e à coleção")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
      "Add to mixer and play")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
      "Filtrar pelo núcleo atual")
MSG_HASH(
      MSG_AUDIO_MIXER_VOLUME,
      "Volume global do misturador de som"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
      "Volume global do misturador de som (em dB). O valor normal são 0 dB, sem ganho aplicado."
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
      "Nível de volume do misturador de som (dB)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
      "Silenciar misturador de som"
      )
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
      "Ativar/desativar 'Silenciar misturador de som'.")
MSG_HASH(MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER,
      "Mostrar 'Atualizador online'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
      "Mostrar/esconder a opção 'Atualizador online'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
      "Mostrar 'Atualizador de núcleos'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
      "Mostrar/esconder a capacidade de atualização de núcleos (e ficheiros de informação de núcleos).")
MSG_HASH(MSG_PREPARING_FOR_CONTENT_SCAN,
      "Preparando a verificação de conteúdo...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_DELETE,
      "Remover núcleo")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_DELETE,
      "Remove este núcleo do disco.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
      "Renomeia o título da entrada.")
MSG_HASH(MENU_ENUM_LABEL_RENAME_ENTRY,
      "Renomear")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
      "Opacidade do buffer de fotogramas")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
      "Altera a opacidade do buffer de fotogramas.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
      "Favoritos")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
      "O conteúdo que foi adicionado aos 'Favoritos' irá aparecer aqui.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
      "Música")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_MUSIC,
      "As músicas que foram tocadas anteriormente irão aparecer aqui.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
      "Imagem")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_IMAGES,
      "As imagens que foram pré-visualizadas anteriormente irão aparecer aqui.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
      "Vídeo")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_VIDEO,
      "Os vídeos que foram reproduzidos anteriormente irão aparecer aqui.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
      "Ícones do menu")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
      "Ativar/desativar os ícones do menu que so mostrados no lado esquerdo das entradas do menu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
      "Ativar o separador Definições")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
      "Introduza uma palavra-passe para ativar o separador 'Definições'")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
      "Introduza a palavra-passe")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
      "Palavra-passe correta.")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
      "Palavra-passe incorreta.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
      "Ativar o separador 'Definições'. Um reinício é necessrio para que as alterações surtam efeito.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
      "Fornecer uma palavra-passe durante a ocultação do separador das definições faz com que seja possível restaurá-lo mais tarde a partir do menu, através do separador Menu principal, selecionando o separador Ativar definições e introduzindo a palavra-passe.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
      "Permitir que o utilizador renomeie entradas nas listas de reprodução.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
      "Permitir renomeação de entradas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
      "Mostrar 'Carregar núcleo'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
      "Mostrar/esconder a opção 'Carregar núcleo'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
      "Mostrar 'Carregar conteúdo'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
      "Mostrar/esconder a opção 'Carregar conteúdo'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
      "Mostrar 'Informação'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
      "Mostrar/esconder a opção 'Informação'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
      "Mostrar 'Configurações'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
      "Mostrar/esconder a opção 'Configurações'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
      "Mostrar 'Ajuda'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
      "Mostrar/esconder a opção 'Ajuda'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
      "Mostrar 'Sair do RetroArch'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
      "Mostrar/esconder a opção 'Sair do RetroArch'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
      "Mostrar 'Reiniciar'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
      "Mostrar/esconder a opção 'Reiniciar'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
      "Show Shutdown")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
      "Show/hide the 'Shutdown' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
      "Menu rápido")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
      "Mostrar ou esconder elementos no 'Menu rápido'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "Mostrar 'Tirar captura de ecrã'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "Mostrar/esconder a opção 'Tirar captura de ecrã'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "Mostrar 'Gravação/Carregamento de estado'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "Mostrar/esconder as opções de gravação/carregamento de estado.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "Mostrar 'Desfazer gravação/carregamento de estado'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "Mostrar/esconder as opções para desfazer carregamento/gravação de estado.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "Mostrar 'Adicionar aos favoritos'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "Mostrar/esconder a opção 'Adicionar aos favoritos'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
      "Show Start Recording")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
      "Show/hide the 'Start Recording' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
      "Show Start Streaming")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
      "Show/hide the 'Start Streaming' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "Show Reset Core Association")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "Show/hide the 'Reset Core Association' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
      "Mostrar 'Opções'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
      "Mostrar/esconder a opção 'Opções'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
      "Mostrar teclas")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
      "Mostrar/esconder a opção 'Teclas'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
      "Mostrar batota")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
      "Mostrar/esconder a opção 'Batota'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
      "Mostrar 'Shaders'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
      "Mostrar/esconder a opção 'Shaders'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "Mostrar 'Guardar sobreposições de núcleos'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "Mostrar/esconder a opção 'Guardar sobreposições de núcleos'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "Mostrar 'Guardar sobreposições de jogos'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "Mostrar/esconder a opção 'Guardar Sobreposições de jogos'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
      "Mostrar 'Informação'")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
      "Mostrar/esconder a opção 'Informação'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
      "Desativar 'Modo quiosque'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
      "Desativa o 'Modo quiosque'. É necessário um reinício para que as alterações surtam efeito.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
      "Ativar 'Modo quiosque'")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
      "Protege o sistema através da ocultação de todas as configurações relacionadas com definições.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
      "Introduza a palavra-passe para desativar o modo quiosque")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
      "Fornecer uma palavra-passe durante a ativação do modo Quiosque faz com que seja possível desativar esse modo a partir do menu, utilizando o Menu Principal, selecionando Desativar modo quiosque e introduzindo a palavra-passe.")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD,
      "Introduza a palavra-passe")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
      "Palavra-passe correta.")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
      "Palavra-passe incorreta.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
      "Adicionar conteúdo à lista de reprodução automaticamente")
MSG_HASH(MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
      "Verificar automaticamente conteúdo carregado de forma a que apareça nas listas de reprodução.")
MSG_HASH(MSG_SCANNING_OF_FILE_FINISHED,
      "Leitura do ficheiro terminada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
      "Qualidade de reamostragem de som")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
      "Baixe este valor para favorecer o desempenho ou baixar a latência sobre a qualidade do som, ou aumente caso pretenda melhor qualidade de som a troco de desempenho/baixa latência.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
      "Display Statistics")
MSG_HASH(MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
      "Show onscreen technical statistics.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
      "Enable border filler")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
      "Enable border filler thickness")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
      "Enable background filler thickness")
MSG_HASH(MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION, "For CRT displays only. Attempts to use exact core/game resolution and refresh rate.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION, "CRT SwitchRes")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
      "Switch among native and ultrawide super resolutions."
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER, "CRT Super Resolution")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
      "Show Rewind Settings")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
      "Show/hide the Rewind options.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
      "Show/hide the Latency options.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
      "Show Latency Settings")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
      "Show/hide the Overlay options.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
      "Show Overlay Settings")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
      "Enable menu audio")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
      "Enable or disable menu sound.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
      "Mixer Settings")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
      "View and/or modify audio mixer settings.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
      "Overrides")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
      "Options for overriding the global configuration.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
      "Will start playback of the audio stream. Once finished, it will remove the current audio stream from memory.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
      "Will start playback of the audio stream. Once finished, it will loop and play the track again from the beginning.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
      "Will start playback of the audio stream. Once finished, it will jump to the next audio stream in sequential order and repeat this behavior. Useful as an album playback mode.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
      "This will stop playback of the audio stream, but not remove it from memory. You can start playing it again by selecting 'Play'.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
      "This will stop playback of the audio stream and remove it entirely from memory.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
      "Adjust the volume of the audio stream.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
      "Add this audio track to an available audio stream slot. If no slots are currently available, it will be ignored.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
      "Add this audio track to an available audio stream slot and play it. If no slots are currently available, it will be ignored.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
      "Play")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
      "Play (Looped)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
      "Play (Sequential)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
      "Stop")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
      "Remove")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
      "Volume")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
      "Current core")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
      "Clear")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
      "In-Menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
      "In-Game")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
      "In-Game (Paused)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
      "Playing")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
      "Paused")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
      "Enable Discord"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
      "Enable or disable Discord support. Will not work with the browser version, only native desktop client."
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
      "Power Management")
MSG_HASH(MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
      "Change power management settings.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
      "Sustained Performance Mode")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
      "mpv support")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Adaptive Vsync"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "V-Sync is enabled until performance falls below the target refresh rate. Can minimize stuttering when performance falls below realtime, and can be more energy efficient."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "Output native, low-resolution signals for use with CRT displays."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Cycle through these options if the image is not centered properly on the display."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "X-Axis Centering"
    )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "Use a custom refresh rate specified in the config file if needed.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "Use Custom Refresh Rate")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
      "Select the output port connected to the CRT display.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
      "Output Display ID")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Start Recording"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Starts recording."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Stop Recording"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Stops recording."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Start Streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Starts streaming."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Stop Streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Stops streaming."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Record Quality"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Stream Quality"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "Streaming URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "UDP Stream Port"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH,
    "Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE,
    "YouTube"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
      "Twitch Stream Key")
MSG_HASH(MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
      "YouTube Stream Key")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
      "Streaming Mode")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
      "Title of Stream")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
    "Split Joy-Con"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Reset To Defaults"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Reset the current configuration to default values."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
    "OK"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
      "Tema da cor do menu")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
    "Basic White"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
    "Basic Black"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Select a different color theme."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Use preferred system color theme")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Use your operating system's color theme (if any) - overrides theme settings.")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWEST,
      "Lowest")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWER,
      "Lower")
MSG_HASH(MSG_RESAMPLER_QUALITY_NORMAL,
      "Normal")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHER,
      "Higher")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHEST,
      "Highest")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "No music available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "No videos available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "No images available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "No favorites available."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
      "Remember Window Position and Size")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
    "CoreAudio support"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
    "CoreAudio V3 support"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
      "Menu Widgets")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
      "Video Shaders")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
      "Scan without core match")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
      "When disabled, content is only added to playlists if you have a core installed that supports its extension. By enabling this, it will add to playlist regardless. This way, you can install the core you need later on after scanning.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
      "Animation Horizontal Icon Highlight")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
      "Animation Move Up/Down")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
      "Animation Main Menu Opens/Closes")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Disc Information"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "View information about inserted media discs."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Frontend Logging Level"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Sets log level for the frontend. If a log level issued by the frontend is below this value, it is ignored."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
      "Framerate Update Interval (in frames)")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
      "Framerate display will be updated at the set interval (in frames).")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Show Restart Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Show/hide the 'Restart Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Show Close Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Show/hide the 'Close Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Show Resume Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Show/hide the 'Resume Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Show Input"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Show or hide 'Input Settings' on the Settings screen."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "AI Service"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "Change settings for the AI Service (Translation/TTS/Misc)."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
      "AI Service Output")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
      "AI Service URL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
      "AI Service Enabled")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
      "Pauses gameplay during translation (Image mode), or continues to run (Speech mode)")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
      "A http:// url pointing to the translation service to use.")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
      "Enable AI Service to run when the AI Service hotkey is pressed.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
      "Target Language")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
      "The language the service will translate to. If set to 'Don't Care', it will default to English.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
      "Source Language")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
      "The language the service will translate from. If set to 'Don't Care', it will attempt to auto-detect the language. Setting it to a specific language will make the translation more accurate.")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "Czech"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "Danish"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "Swedish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "Croatian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "Catalan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "Bulgarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "Bengali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "Basque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "Azerbaijani"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "Albanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "Afrikaans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "Estonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
   "Filipino"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "Finnish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "Galician"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "Georgian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "Gujarati"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "Haitian Creole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "Hebrew"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HINDI,
   "Hindi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "Hungarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "Icelandic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "Indonesian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "Irish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "Kannada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATIN,
   "Latin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "Latvian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "Lithuanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "Macedonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "Malay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "Maltese"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
   "Norwegian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
   "Persian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "Romanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "Serbian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "Slovak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "Slovenian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "Swahili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "Tamil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
   "Telugu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "Thai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "Ukrainian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_URDU,
   "Urdu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "Welsh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "Yiddish"
   )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_DISC,
      "Load a physical media disc. You should first select the core (Load Core)  you intend to use with the disc.")
MSG_HASH(MENU_ENUM_SUBLABEL_DUMP_DISC,
      "Dump the physical media disc to internal storage. It will be saved as an image file.")
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Image Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Speech Mode"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
      "Remove")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
      "Remove shader presets of a specific type.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Remove Global Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Remove the Global Preset, used by all content and all cores.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Remove Core Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Remove the Core Preset, used by all content ran with the currently loaded core.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Remove Content Directory Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Remove the Content Directory Preset, used by all content inside the current working directory.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Remove Game Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Remove the Game Preset, used only for the specific game in question.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
      "Frame Time Counter")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
      "Adjust settings influencing the frame time counter (only active when threaded video is disabled).")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
      "Use modern decorated animations, notifications, indicators and controls instead of the old text only system.")
