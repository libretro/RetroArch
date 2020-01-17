#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
    "Overclock da GPU"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
    "Faz um Overclock ou underclock na CPU do Switch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL,
    "Brilho da tela"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_BACKLIGHT_CONTROL,
    "Aumenta ou diminui o brilho da tela do Switch"
    )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
    "Overclock da CPU"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
    "Faz um Overclock na CPU do Switch"
    )
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
    "Nativo"
    )
MSG_HASH(
    MSG_DEVICE_DISCONNECTED_FROM_PORT,
    "Dispositivo desconectado da porta"
    )
MSG_HASH(
    MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
    "Recebido um comando de jogo em rede desconhecido"
    )
MSG_HASH(
    MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
    "Este arquivo já existe. Salvando no buffer de backup"
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM,
    "Conexão recebida de: \"%s\""
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM_NAME,
    "Conexão recebida de: \"%s (%s)\""
    )
MSG_HASH(
    MSG_PUBLIC_ADDRESS,
    "Sucesso no Mapeamento de Portas"
    )
MSG_HASH(
    MSG_UPNP_FAILED,
    "Falha no Mapeamento de Portas"
    )
MSG_HASH(
    MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
    "Nenhum argumento fornecido e nenhum menu interno, exibindo ajuda..."
    )
MSG_HASH(
    MSG_SETTING_DISK_IN_TRAY,
    "Definindo disco na bandeja"
    )
MSG_HASH(
    MSG_WAITING_FOR_CLIENT,
    "Aguardando pelo cliente..."
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
    "Você deixou o jogo"
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
    "Você se juntou como jogador %u"
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
    "Você se juntou ao dispositivo de entrada %.*s"
    )
MSG_HASH(
    MSG_NETPLAY_PLAYER_S_LEFT,
    "O jogador %.*s deixou o jogo"
    )
MSG_HASH(
    MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
    "%.*s se juntou como jogador %u"
    )
MSG_HASH(
    MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
    "%.*s juntou-se aos dispositivos de entrada %.*s"
    )
MSG_HASH(
    MSG_NETPLAY_NOT_RETROARCH,
    "Uma tentativa de conexão de jogo em rede falhou porque o par não está executando o RetroArch ou está executando uma versão antiga do RetroArch."
    )
MSG_HASH(
    MSG_NETPLAY_OUT_OF_DATE,
    "O par de jogo em rede está executando uma versão antiga do RetroArch. Não é possível conectar."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_VERSIONS,
    "ATENÇÃO: Um par de jogo em rede está executando uma versão diferente do RetroArch. Se ocorrerem problemas, use a mesma versão."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORES,
    "Um par de jogo em rede está executando um núcleo diferente. Não é possível conectar."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
    "ATENÇÃO: Um par de jogo em rede está executando uma versão diferente do núcleo. Se ocorrerem problemas, use a mesma versão."
    )
MSG_HASH(
    MSG_NETPLAY_ENDIAN_DEPENDENT,
    "Este núcleo não suporta jogo em rede entre diferentes arquiteturas de sistemas"
    )
MSG_HASH(
    MSG_NETPLAY_PLATFORM_DEPENDENT,
    "Este núcleo não suporta jogo em rede entre diferentes sistemas"
    )
MSG_HASH(
    MSG_NETPLAY_ENTER_PASSWORD,
    "Digite a senha do servidor de jogo em rede:"
    )
MSG_HASH(
    MSG_DISCORD_CONNECTION_REQUEST,
    "Deseja permitir a conexão do usuário:"
    )
MSG_HASH(
    MSG_NETPLAY_INCORRECT_PASSWORD,
    "Senha incorreta"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_NAMED_HANGUP,
    "\"%s\" desconectou"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_HANGUP,
    "Um cliente de jogo em rede desconectou"
    )
MSG_HASH(
    MSG_NETPLAY_CLIENT_HANGUP,
    "Desconectado do jogo em rede"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
    "Você não tem permissão para jogar"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
    "Não há vagas livres para jogadores"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
    "Os dispositivos de entrada solicitados não estão disponíveis"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY,
    "Impossível alterar para modo jogador"
    )
MSG_HASH(
    MSG_NETPLAY_PEER_PAUSED,
    "Par do jogo em rede \"%s\" pausou"
    )
MSG_HASH(
    MSG_NETPLAY_CHANGED_NICK,
    "Seu apelido mudou para \"%s\""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
    "Concede aos núcleos renderizados por hardware seu próprio contexto privado. Evita ter que assumir mudanças de estado de hardware entre quadros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
    "Ativa a animação horizontal para o menu. Isso terá um impacto no desempenho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SETTINGS,
    "Ajusta as configurações de aparência da tela de menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
    "Sincronia rígida entre CPU e GPU. Reduz a latência ao custo de desempenho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_THREADED,
    "Melhora o desempenho ao custo de latência e mais engasgamento de vídeo. Use somente se você não puder obter velocidade total de outra forma."
    )
MSG_HASH(
    MSG_AUDIO_VOLUME,
    "Volume de áudio"
    )
MSG_HASH(
    MSG_AUTODETECT,
    "Detectar automaticamente"
    )
MSG_HASH(
    MSG_AUTOLOADING_SAVESTATE_FROM,
    "Autocarregando Estado de Jogo de"
    )
MSG_HASH(
    MSG_CAPABILITIES,
    "Capacidades"
    )
MSG_HASH(
    MSG_CONNECTING_TO_NETPLAY_HOST,
    "Conectando ao anfitrião de jogo em rede"
    )
MSG_HASH(
    MSG_CONNECTING_TO_PORT,
    "Conectando a porta"
    )
MSG_HASH(
    MSG_CONNECTION_SLOT,
    "Vaga de conexão"
    )
MSG_HASH(
    MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
    "Desculpe, não implementado: núcleos que não exigem conteúdo não podem participar do jogo em rede."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
    "Senha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
    "Contas Cheevos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
    "Nome de usuário"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
    "Contas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
    "Ponto Final da Lista de Contas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
    "Lista de Conquistas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
    "Pausar Conquistas no Modo Hardcore"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
    "Continuar Conquistas no Modo Hardcore"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
    "Lista de Conquistas (Hardcore)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
    "Analisar Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
    "Arquivo de Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TAB,
    "Importar conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
    "Salas de Jogo em Rede"
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
    "Bloquear Quadros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
    "Dispositivo de Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
    "Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
    "Plugin DSP de Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
    "Habilitar Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
    "Filtro de Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
    "Turbo/Zona-Morta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
    "Latência de Áudio (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
    "Desvio Máximo de Tempo do Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
    "Silenciar Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
    "Taxa da Saída de Áudio (Hz)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
    "Controle Dinâmico da Taxa de Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
    "Reamostragem de Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
    "Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
    "Sincronizar Áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
    "Nível de Volume de Áudio (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "WASAPI Modo Exclusivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
    "WASAPI Formato de Ponto Flutuante"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "WASAPI Tamanho do Buffer Compartilhado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
    "Intervalo do Salvamento Automático da SRAM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
    "Carrega Automaticamente Arquivos de Redefinição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
    "Carrega Automaticamente Arquivos de Remapeamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
    "Usar Arquivo de Opções Globais de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
    "Salva todas as opções de núcleo em um arquivo de configuração comum (retroarch-core-options.cfg). Quando desabilitadas, as opções para cada núcleo são salvas em uma pasta/arquivo específico do núcleo, separado no diretório 'Config' do RetroArch."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
    "Carrega Automaticamente Predefinições de Shader"
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
    "Rolar para Baixo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
    "Rolar para Cima"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
    "Iniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
    "Alternar Teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
    "Alternar Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
    "Controles Básicos de Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
    "Confirmar/OK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
    "Informação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
    "Sair"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
    "Rolar para Cima"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
    "Padrões"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
    "Alternar Teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
    "Alternar Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
    "Não Sobregravar a SRAM ao Carregar Estado de Jogo"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
    "Habilitar Bluetooth"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
    "URL de Recursos do Buildbot"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
    "Cache"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
    "Permitir Câmera"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
    "Câmera"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT,
    "Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
    "Aplicar Alterações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
    "Iniciar Pesquisa Por um Novo Código de Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
    "Continuar Pesquisa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
    "Arquivo de Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
    "Arquivo de Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
    "Carregar Arquivo de Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
    "Carregar Arquivo de Trapaça (Anexado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
    "Salvar Arquivo de Trapaça Como"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
    "Estágios de Trapaça"
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
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
    "Tabelas de Classificação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
    "Insígnias de Conquistas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
    "Bloqueada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
    "Testar Conquistas Não Oficiais"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
    "Desbloqueada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
    "Hardcore"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
    "Modo Detalhado das Conquistas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
    "Captura de Conquistas Automática"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
    "Fechar Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG,
    "Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
    "Carregar Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
    "Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
    "Salvar Configuração ao Sair"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
    "Base de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
    "Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
    "Tamanho da Lista de Histórico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
    "Tamanho da Lista de Favoritos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
    "Limita o número de entradas na lista de reprodução de favoritos. Uma vez atingido o limite, novas adições serão evitadas até que as entradas antigas sejam removidas. Definir um valor -1 permite entradas 'ilimitadas' (99999). AVISO: A redução do valor excluirá as entradas existentes!"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
    "Permitir a remoção de itens"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
    "Menu Rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
    "Recursos de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
    "Downloads"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
    "Trapaças"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
    "Contadores do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
    "Exibir nome do núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
    "Informação do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
    "Autores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
    "Categorias"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
    "Rótulo do núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
    "Nome do núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
    "Firmware(s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
    "Licença(s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
    "Permissões"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
    "Extensões suportadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
    "Fabricante do sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
    "Nome do sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
    "API gráficas requeridas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
    "Controles"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_LIST,
    "Carregar Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
    "Instalar ou Restaurar um Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
    "Falha na instalação do núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
    "Instalação do núcleo bem-sucedida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
    "Opções"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
    "Iniciar um Núcleo Automaticamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "Extrair automaticamente o arquivo baixado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
    "URL de Núcleos do Buildbot"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
    "Atualizador de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
    "Atualizador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
    "Arquitetura da CPU:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_CORES,
    "Cores da CPU:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
    "Cursor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
    "Gerenciar Cursor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
    "Proporção Personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
    "Gerenciar Base de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
    "Seleção de Base de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
    "Remover"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES,
    "Diretório Inicial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
    "<Diretório de Conteúdo>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
    "<Padrão>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
    "<Nenhum>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
    "Diretório não encontrado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
    "Diretório"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_INDEX,
    "Índice de Disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
    "Controle de Disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DONT_CARE,
    "Não importa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
    "Downloads"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
    "Baixar Núcleo..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
    "Download de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
    "Fator de escala do menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
    "Aplica um valor global na escala do menu. Pode ser usado para aumentar ou diminuir o tamanho da interface do usuário."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
    "Driver"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
    "Carregar Vazio ao Fechar o Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
    "Verificar por Firmware que Falta Antes de Carregar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
    "Plano de Fundo Dinâmico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Planos de Fundo Dinâmicos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
    "Habilitar Conquistas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FALSE,
    "Falso"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
    "Velocidade Máxima de Execução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
    "Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FPS_SHOW,
    "Mostrar Taxa de Quadros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
    "Incluir Detalhes da Memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
    "Controlar Velocidade Máxima de Execução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
    "Sincronizar Taxa de Atualização Exata ao Conteúdo (G-Sync, FreeSync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
    "Controle de Quadros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
    "Contadores do Frontend"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
    "Carrega Automaticamente Opções de Núcleo Específicas do Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
    "Criar arquivo de opções do jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
    "Salvar arquivo de opções do jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP,
    "Ajuda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
    "Solução de Problemas de Áudio/Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
    "Alterando a Sobreposição do Gamepad Virtual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
    "Controles Básicos de Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LIST,
    "Ajuda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
    "Carregando Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
    "Procurando em Busca de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
    "O Que é um Núcleo?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
    "Habilitar Lista de Histórico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
    "Histórico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
    "Menu Horizontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
    "Imagem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION,
    "Informação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
    "Informação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
    "Tipo de Analógico Para Digital"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
    "Todos os Usuários Controlam o Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
    "Analógico Esquerdo X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
    "Analógico Esquerdo X- (esquerda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
    "Analógico Esquerdo X+ (direita)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
    "Analógico Esquerdo Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
    "Analógico Esquerdo Y- (cima)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
    "Analógico Esquerdo Y+ (baixo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
    "Analógico Direito X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
    "Analógico Direito X- (esquerda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
    "Analógico Direito X+ (direita)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
    "Analógico Direito Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
    "Analógico Direito Y- (cima)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
    "Analógico Direito Y+ (baixo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
    "Gatinho da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
    "Recarregar Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
    "Aux A da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
    "Aux B da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
    "Aux C da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
    "Start da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
    "Select da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
    "D-pad Cima da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
    "D-pad Baixo da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
    "D-pad Esquerdo da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
    "D-pad Direito da Pistola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
    "Habilitar Auto Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
    "Limite do Eixo do Botão da Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
    "Zona Morta do Controle Analógico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
    "Sensibilidade do Controle Analógico"
    )
#ifdef GEKKO
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
    "Escala do Mouse"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
    "Ajusta a escala X/Y para a velocidade das lightguns usando o Wiimote."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
    "Inverter Botões OK e Cancelar do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
    "Vincular Todos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
    "Vincular Todos pelo Padrão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
    "Tempo Limite para Vincular"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
    "Vincular (Manter Pressionado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
    "Tempo Limite de Bloqueio de Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
    "Ocultar Descritores de Entrada do Núcleo Não Vinculados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
    "Exibir Rótulos do Descritor de Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
    "Índice de Dispositivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
    "Tipo de Dispositivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
    "Índice de Mouse"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
    "Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
    "Ciclo de Trabalho"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
    "Vínculos das Teclas de Atalho da Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
    "Habilitar Mapeamento de Gamepad no Teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
    "Botão A (direita)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
    "Botão B (baixo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
    "Direcional para baixo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
    "Botão L2 (gatilho)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
    "Botão L3 (polegar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
    "Botão L (ombro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
    "Direcional Esquerdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
    "Botão R2 (gatilho)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
    "Botão R3 (polegar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
    "Botão R (ombro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
    "Direcional Direito"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
    "Botão Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
    "Botão Start"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
    "Direcional para Cima"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
    "Botão X (topo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
    "Botão Y (esquerda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_KEY,
    "(Tecla: %s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
    "Mouse 1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
    "Mouse 2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
    "Mouse 3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
    "Mouse 4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
    "Mouse 5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
    "Roda do Mouse para Cima"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
    "Roda do Mouse para Baixo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
    "Roda do Mouse para Esquerda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
    "Roda do Mouse para Direita"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
    "Tipo de Mapeamento para Gamepad no Teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
    "Usuários Máximos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Combinação do Gamepad para Alternar Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
    "Índice de Trapaça -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
    "Índice de Trapaça +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
    "Alternar Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
    "Alternar ejeção de disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
    "Próximo disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
    "Disco anterior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
    "Habilitar teclas de atalho"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
    "Manter Avanço Rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
    "Alternar Avanço Rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
    "Avanço de Quadro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
    "Enviar informações de depuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
    "Alternar FPS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
    "Alternar hspedagem de jogo em rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
    "Alternar tela cheia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
    "Alternar captura do Mouse"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
    "Alternar foco do jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
    "Alternar menu desktop"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
    "Carregar Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
    "Alternar menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
    "Alternar gravação de filme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
    "Alternar áudio mudo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
    "Alternar modo jogador/espectador do jogo em rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
    "Alternar teclado virtual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
    "Próxima Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
    "Alternar pausa"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "Reiniciar o RetroArch"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "Sair do RetroArch"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
    "Reiniciar jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
    "Rebobinar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
    "Detalhes da Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
    "Iniciar ou Continuar a Pesquisa de Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
    "Salvar Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
    "Capturar tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
    "Próximo shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
    "Shader anterior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
    "Manter câmera lenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
    "Alternar câmera lenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
    "Compartimento do Estado de Jogo -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
    "Compartimento do Estado de Jogo +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
    "Volume -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
    "Volume +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
    "Mostrar Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
    "Ocultar Sobreposição no Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Exibir Comandos Na Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Exibir Cursor do Mouse com Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
    "Girar Automaticamente a Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
    "Se suportado pela sobreposição atual, girar automaticamente o esquema para ajustar com a orientação e proporção da tela."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Porta de Escuta do Exibir Comandos "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
    "Tipo de Comportamento da Chamada Seletiva"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
    "Mais cedo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
    "Mais tarde"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
    "Normal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
    "Preferir Toque Frontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
    "Remapeamento de Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
    "Remapear Vínculos para este núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
    "Salvar Auto Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
    "Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
    "Habilitar Teclado Pequeno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
    "Habilitar Toque"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
    "Habilitar Turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
    "Período do Turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS, /* TODO/FIXME - Change user to port */
    "Vínculos de Entrada do Usuário %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
    "Latência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
    "Condição do armazenamento interno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
    "Auto Configuração de Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
    "Joypad"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
    "Serviços"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
    "Chinês (Simplificado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
    "Chinês (Tradicional)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
    "Holandês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
    "Inglês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
    "Esperanto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
    "Francês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
    "Alemão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
    "Italiano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
    "Japonês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
    "Coreano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_POLISH,
    "Polonês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
    "Português (Brasil)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
    "Português (Portugal)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
    "Russo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
    "Espanhol"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
    "Vietnamita"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
    "Árabe"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GREEK,
    "Grego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
    "Turco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
    "Analógico Esquerdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
    "Informação do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
    "Nível de Registro de Eventos do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LINEAR,
    "Linear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
    "Carregar Arquivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
    "Carregar Recente"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
    "Seleciona o conteúdo do histórico recente da lista de reprodução."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
    "Carregar Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_DISC,
    "Carregar Disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DUMP_DISC,
    "Dumpar Disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_STATE,
    "Carregar Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
    "Permitir Localização"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
    "Localização"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
    "Registro de Eventos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
    "Verbosidade do Registro de Eventos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
    "Registrar em Arquivo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_TO_FILE,
    "Redireciona as mensagens do registro de eventos do sistema para o arquivo. Requer 'Verbosidade do Registro de Eventos' para ser ativado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
    "Arquivos de Registro Com de Data e Hora"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
    "Redireciona a saída de cada sessão do RetroArch para um novo arquivo com registro de data e hora. Desativar irá sobregravar o registro sempre que o RetroArch for reiniciado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MAIN_MENU,
    "Menu Principal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANAGEMENT,
    "Configurações da Base de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
    "Tema de Cor do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
    "Azul"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
    "Cinza Azulado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
    "Azul Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
    "Verde"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
    "Nvidia Shield"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
    "Vermelho"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
    "Amarelo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI,
    "Material UI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
    "Material UI Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
    "Ozone Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD,
    "Nórdico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
    "Gruvbox Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
    "Solarized Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
    "Cutie Azul"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
    "Cutie Turquesa"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
    "Cutie Verde"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
    "Cutie Laranja"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
    "Cutie Roza"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
    "Cutie Roxo"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
    "Cutie Vermelho"
    )
  MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY,
    "Virtual Boy"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
    "Transições Animadas do Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
    "Habilita os efeitos de animação ao navegar entre diferentes opções de menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
    "Automáticas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
    "Desvanecer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
    "Deslizar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
    "Desabilitar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
    "Miniaturas no Modo Retrato"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
    "Especifica como exibir miniaturas das listas da reprodução na orientação de retrato."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
    "Miniaturas no Modo Paisagem"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
    "Especifica como exibir miniaturas das das listas da reprodução na orientação da paisagem."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
    "DESABILITAR"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
    "Lista (Pequena)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
    "Lista (Média)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
    "DESABILITAR"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
    "Lista (Pequena)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
    "Lista (Média)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
    "Lista (Grande)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
    "Opacidade do Rodapé"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
    "Opacidade do Cabeçalho"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
    "Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
    "Controlar Taxa de Quadros do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
    "Explorador de Arquivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
    "Filtro Linear de Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
    "Adiciona um leve desfoque no menu para remover as arestas de pixel rígido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
    "Animação Horizontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
    "Aparência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
    "Plano de Fundo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
    "Opacidade do plano de fundo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MISSING,
    "Faltando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MORE,
    "..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
    "Suporte para Mouse"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
    "Multimídia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
    "Música"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Filtrar Extensões Desconhecidas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
    "Navegação Retorna ao Início"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NEAREST,
    "Mais Próximo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY,
    "Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
    "Permitir Clientes em Modo Escravo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
    "Verificar Quadros do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "Quadros de Latência de Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "Faixa de Quadros de Latência de Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
    "Atraso de Quadros do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
    "Desconectar do anfitrião de Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
    "Habilitar Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
    "Conectar ao Anfitrião de Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
    "Iniciar Anfitrião de Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
    "Parar Anfitrião de Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
    "Endereço do Servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
    "Analisar a rede local"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
    "Habilitar Cliente de Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
    "Usuário"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
    "Senha do Servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
    "Anunciar Jogo em Rede Publicamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
    "Solicitar Dispositivo %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
    "Não Permitir Clientes em Modo Não Escravo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
    "Configurações do jogo em rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
    "Compartilhamento de Entrada Analógica"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
    "Máximo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
    "Médio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
    "Compartilhamento de Entrada Digital"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
    "Compartilhar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
    "Agarrar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
    "Eleger"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
    "Nenhum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
    "Sem preferência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
    "Modo Espectador do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
    "Modo sem Estados de Jogo do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
    "Senha Apenas Espectador do Servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
    "Habilitar Espectador do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
    "Porta TCP do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
    "Travessia de NAT do Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
    "Comandos de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
    "Porta de Comando de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
    "Informação de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
    "Gamepad de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
    "Porta Base Remota de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
    "Baixar Miniaturas Sob Demanda"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
    "Faça o download automático de imagens em miniatura ausentes ao navegar pelas listas de reprodução. Tem um grande impacto no desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
    "Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO,
    "Não"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NONE,
    "Nenhum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
    "N/D"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
    "Não há Conquistas para mostrar."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE,
    "Sem Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
    "Nenhum núcleo disponível"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
    "Não há informação de núcleo disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
    "Não há opções de núcleo disponíveis."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
    "Não há itens para mostrar."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
    "Não há histórico disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
    "Não há informação disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ITEMS,
    "Sem itens."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
    "Nenhum anfitrião de jogo em rede encontrado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
    "Nenhuma rede encontrada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
    "Não há contadores de desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
    "Não há listas de reprodução."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
    "Não há itens de lista de reprodução disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
    "Nenhuma configuração encontrada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
    "Nenhuma predefinição automática de shader encontrada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
    "Não há parâmetros de Shader."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OFF,
    "DESLIGADO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ON,
    "LIGADO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE,
    "On-line"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
    "Atualizador On-line"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
    "Exibição na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
    "Sobreposição na Tela"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
    "Opções de controles de Notificações na Tela ou Molduras"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
    "Esquema de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
    "Ajustar o Esquema de Vídeo"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Notificações na Tela"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Ajusta as Notificações na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
    "Navegar no Arquivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPTIONAL,
    "Opcional"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY,
    "Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
    "Carrega Automaticamente Sobreposição Favorita"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
    "Sobreposição"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
    "Esquema de Vídeo"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
    "Opacidade da Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
    "Predefinição de Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
    "Escala da Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
    "Sobreposição na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
    "Utilizar Modo PAL60"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
    "Diretório superior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
    "Ativar acesso a arquivos externos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
    "Abre as configurações de permissões de acesso a arquivos do Windows"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
    "Abrir..."
)
MSG_HASH(
    MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
    "Abre outro diretório usando o seletor de arquivos do sistema"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
    "Pausar quando o menu for ativado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
    "Continuar o conteúdo depois de usar os estados salvos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
    "Feche automaticamente o menu e continue o conteúdo atual depois de selecionar 'Salvar Estado de Jogo' ou 'Carregar Estado de Jogo' no menu rápido. Desativar isso pode melhorar o desempenho ao salvar um jogo em dispositivos muito lentos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
    "Não rodar em segundo plano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
    "Contadores de Desempenho"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
    "Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
    "Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
    "Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
    "Gerenciamento de Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
    "Executa tarefas de manutenção na lista de reprodução selecionada (ex: definir/restaurar associações padrões do núcleo)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
    "Núcleo Padrão"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
    "Especifique o núcleo a ser usado ao iniciar o conteúdo por meio de uma entrada de lista de reprodução que não tenha uma associação principal existente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
    "Restaurar Associações do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
    "Remova as associações existentes dos núcleos para todas as entradas da lista de reprodução."
    )
MSG_HASH(
    MSG_PLAYLIST_MANAGER_RESETTING_CORES,
    "Restaurando núcleos: "
    )
MSG_HASH(
    MSG_PLAYLIST_MANAGER_CORES_RESET,
    "Restauração dos núcleos: "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Modo de Exibição do Rótulo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Altere como os rótulos de conteúdo são exibidos nesta lista de reprodução."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
    "Mostrar rótulos completos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
    "Remover ()"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
    "Remover []"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
    "Remover () e []"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
    "Manter região"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
    "Manter o índice do disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
    "Manter a região e o índice do disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
    "Padrão do Sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
    "Suporte para Toque"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PORT,
    "Porta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRESENT,
    "Presente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
    "Privacidade"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
    "MIDI"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "Reiniciar o RetroArch"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "Sair do RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
    "Reiniciar o RetroArch"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
    "Entrada do banco de dados"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
    "Exibir informações do banco de dados do conteúdo atual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
    "Analógico suportado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
    "Classificação BBFC"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
    "Classificação CERO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
    "Cooperativo suportado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
    "CRC32"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
    "Descrição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
    "Desenvolvedor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Edição da Revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
    "Classificação da Revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
    "Análise da Revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
    "Classificação ELSPA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
    "Hardware de Aprimoramento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
    "Classificação ESRB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
    "Classificação da Revista Famitsu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
    "Franquia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
    "Gênero"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
    "MD5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
    "Nome"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
    "Origem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
    "Classificação PEGI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
    "Editor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
    "Mês de Lançamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
    "Ano de Lançamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
    "Suporte para Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
    "Número de Série"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
    "SHA1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
    "Iniciar Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
    "Classificação TGDB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
    "Nome"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
    "Caminho do arquivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
    "Banco de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
    "Tempo de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
    "Última Partida"
    )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Reiniciar (RCM)"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Reiniciar"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
    "Configuração de Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
    "Saída de Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
    "Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
    "Carregar Configuração de Gravação..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
    "Configuração de Transmissão Personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
    "Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
    "MIDI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
    "Habilitar Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_PATH,
    "Salvar Saída de Gravação Como..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
    "Salvar Gravações no Diretório de Saída"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE,
    "Arquivo de Remapeamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
    "Carregar Arquivo de Remapeamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
    "Salvar Arquivo de Remapeamento de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
    "Salvar Remapeamento de Controle para o Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
    "Salvar Arquivo de Remapeamento de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
    "Excluir Arquivo de Remapeamento de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
    "Excluir Arquivo de Remapeamento de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
    "Excluir Arquivo de Remapeamento de Jogo do Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REQUIRED,
    "Obrigatório"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
    "Reiniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME,
    "Continuar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
    "Continuar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
    "RetroKeyboard"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD,
    "RetroPad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
    "RetroPad com Analógico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
    "Conquistas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
    "Habilitar Rebobinagem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
    "Aplicar Após Alternar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
    "Aplicar Automaticamente Trapaças Durante o Carregamento do Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
    "Níveis da Rebobinamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
    "Tamanho do Buffer da Rebobinamento (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
    "Tamanho do Intervalo de Ajuste do Buffer (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
    "Rebobinamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
    "Configurações da Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
    "Detalhes da Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
    "Iniciar ou Continuar a Pesquisa de Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
    "Navegador de Arquivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
    "Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
    "Mostrar Tela Inicial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
    "Analógico Direito"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
    "Adicionar aos Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
    "Adicionar aos Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
    "Baixar Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
    "Faça o download de imagens em miniatura da captura de tela, arte da caixa ou tela de título para o conteúdo atual. Atualiza quaisquer miniaturas existentes."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
    "Definir Associação do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
    "Redefinir Associação do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN,
    "Executar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
    "Executar"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
    "Habilitar SAMBA"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
    "Arquivo de Jogo-Salvo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
    "Índice Automático de Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
    "Carrega Automaticamente Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
    "Salvar Automaticamente Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
    "Arquivo de Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
    "Miniaturas do Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
    "Salvar Configuração Atual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Salvar Redefinição de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Salvar Redefinições do Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Salvar Redefinição de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
    "Salvar Nova Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_STATE,
    "Salvar Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
    "Salvando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
    "Analisar Diretório"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_FILE,
    "Analisar Arquivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
    "<Analisar Este Diretório>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
    "Captura de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
    "Resolução da Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SEARCH,
    "Procurar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SECONDS,
    "segundos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS,
    "Configurações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
    "Configurações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER,
    "Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
    "Aplicar Alterações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
    "Shaders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
    "Faixa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
    "Faixa (simplificada)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
    "Neve Simples"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
    "Neve"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
    "Exibir Configurações Avançadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
    "Exibir Arquivos e Pastas Ocultos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHUTDOWN,
    "Desligar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
    "Taxa de Câmera Lenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
    "Adiantar Quadro para Reduzir a Latência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
    "Número de Quadros para Adiantar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
    "O Adiantamento de Quadro Usará uma Segunda Instância"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
    "Ocultar Avisos do Adiantamento de Quadro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
    "Classificar Arquivos de Jogo-Salvo em Pastas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
    "Classificar Arquivos de Estado de Jogo em Pastas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
    "Gravar Estados de Jogo no Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
    "Gravar Jogos-Salvos no Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
    "Arquivos de Sistema estão no Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
    "Salvar Capturas de Tela no Diretório de Conteúdo"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
    "Habilitar SSH"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_CORE,
    "Iniciar Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
    "Iniciar RetroPad Remoto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
    "Iniciar Processador de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATE_SLOT,
    "Compartimento do Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATUS,
    "Condição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
    "Comandos stdin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
    "Núcleos sugeridos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
    "Desativar Protetor de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
    "Habilitar Música em Segundo Plano do Sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
    "Sistema/BIOS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
    "Informação do Sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
    "Suporte a 7zip"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
    "Suporte a ALSA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
    "Data de compilação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
    "Suporte a Cg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
    "Suporte a Cocoa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
    "Suporte à interface de comando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
    "Suporte a CoreText"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
    "Modelo da CPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
    "Características de CPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
    "Métrica DPI da tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
    "Métrica de altura da tela (mm)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
    "Métrica de largura da tela (mm)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
    "Suporte a DirectSound"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
    "Suporte a WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
    "Suporte à biblioteca dinâmica"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
    "Carregamento dinâmico em tempo de execução da biblioteca Libretro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
    "Suporte a EGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
    "Suporte a OpenGL/Direct3D render-to-texture (multi-pass shaders)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
    "Suporte a FFmpeg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
    "Suporte a FreeType"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
    "Suporte a STB TrueType"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
    "Identificador do Frontend"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
    "Nome do Frontend"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
    "SO do Frontend"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
    "Versão Git"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
    "Suporte a GLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
    "Suporte a HLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
    "Suporte a JACK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
    "Suporte a KMS/EGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
    "Versão Lakka"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
    "Suporte a LibretroDB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
    "Suporte a Libusb"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
    "Suporte de jogo em rede (ponto-a-ponto)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
    "Suporte à Interface de comando de rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
    "Suporte a Gamepad de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
    "Suporte a OpenAL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
    "Suporte a OpenGL ES"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
    "Suporte a OpenGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
    "Suporte a OpenSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
    "Suporte a OpenVG"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
    "Suporte a OSS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
    "Suporte à Sobreposição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
    "Fonte de Energia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
    "Carregado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
    "Carregando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
    "Descarregando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
    "Não há fonte"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
    "Suporte a PulseAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
    "Suporte a Python (suporte de script em Shaders)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
    "Suporte a BMP (RBMP)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
    "Nível RetroRating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
    "Suporte a JPEG (RJPEG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
    "Suporte a RoarAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
    "Suporte a PNG (RPNG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
    "Suporte a RSound"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
    "Suporte a TGA (RTGA)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
    "Suporte a SDL2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
    "Suporte a imagem SDL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
    "Suporte a SDL1.2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
    "Suporte a Slang"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
    "Suporte a Paralelismo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
    "Suporte a Udev"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
    "Suporte a Video4Linux2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
    "Driver de contexto de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
    "Suporte a Vulkan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
    "Suporte a Metal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
    "Suporte a Wayland"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
    "Suporte a X11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
    "Suporte a XAudio2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
    "Suporte a XVideo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
    "Suporte a Zlib"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
    "Capturar tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
    "Paralelismo de tarefas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS,
    "Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
    "Miniatura Acima"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
    "Miniaturas à Esquerda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
    "Miniatura Abaixo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
    "Segunda Miniatura"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
    "Disposição Vertical de Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
    "Fator de Escala de Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
    "Reduz o tamanho da exibição de miniaturas dimensionando a largura máxima permitida."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
    "Limite de Redimensionamento de Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
    "Automaticamente redimensiona imagens em miniatura com uma largura/altura menor do que o valor especificado. Melhora a qualidade da imagem. Tem um impacto moderado no desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
    "Mostrar Miniaturas da Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
    "Ativa a exibição de miniaturas em escala reduzida durante a visualização das listas de reprodução. Quando desativada, a 'Miniatura Acima' ainda pode ser alternada para tela cheia ao pressionar RetroPad Y."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
    "Trocar Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
    "Alterna as posições de exibição de 'Miniatura Acima' e 'Miniatura Abaixo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
    "Atraso das Miniaturas (ms)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
    "Aplica um intervalo de tempo entre a seleção de uma entrada da lista de reprodução e o carregamento de suas miniaturas associadas. Configurar isso para um valor de pelo menos 256 ms permite a rolagem rápida e sem atrasos até mesmo nos dispositivos mais lentos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
    "Método de Redução da Escala de Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
    "Método de reamostragem usado ao reduzir miniaturas para caber na tela."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
    "Vizinho Mais Próximo (Rápido)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
    "Bilinear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
    "Sinc/Lanczos3 (Lento)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
    "Nenhum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
    "Automático"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X2,
    "x2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X3,
    "x3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X4,
    "x4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X5,
    "x5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X6,
    "x6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X7,
    "x7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X8,
    "x8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X9,
    "x9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_4_3,
    "4:3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9,
    "16:9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
    "16:9 (Centralizado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10,
    "16:10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
    "16:10 (Centralizado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
    "DESLIGADO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
    "Ajustar à Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
    "Escala Inteira"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
    "Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
    "Atualizador de Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
    "Baixe o pacote completo de miniaturas para o sistema selecionado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
    "Atualizador de Miniaturas da Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
    "Baixe miniaturas individuais para cada entrada da lista de reprodução selecionada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
    "Arte da Caixa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
    "Capturas de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
    "Telas do Título"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
    "Exibir data e hora"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
     "Estilo da data / hora"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
     "Altera o estilo da data atual ou como a hora é mostrada dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS,
    "YYYY-MM-DD HH:MM:SS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM,
    "YYYY-MM-DD HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY,
    "MM-DD-YYYY HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS,
    "HH:MM:SS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM,
    "HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM,
    "DD/MM HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM,
    "MM/DD HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS_AM_PM,
    "YYYY-MM-DD HH:MM:SS (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM_AM_PM,
    "YYYY-MM-DD HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY_AM_PM,
    "MM-DD-YYYY HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS_AM_PM,
    "HH:MM:SS (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM_AM_PM,
    "HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM_AM_PM,
    "DD/MM HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM_AM_PM,
    "MM/DD HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
    "Animação de Textos Longos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
    "Seleciona o método de rolagem horizontal usado para exibir as sequências de textos longos do menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
    "Salto para a esquerda/direita"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
    "Rolar para a esquerda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
    "Velocidade dos Textos Longos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
    "Velocidade de animação ao rolar longas sequências de texto do menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
    "Suavizar Rolagem dos Textos Longos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
    "Use animação de rolagem suave ao exibir longas sequências de texto de menu. Tem um pequeno impacto no desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
    "Tema da Cor do Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
    "Seleciona um tema de cor diferente. Escolhendo 'Personalizado' permite o uso de arquivos predefinidos do tema de menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
    "Tema de Menu Personalizado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
    "Seleciona um tema de menu no navegador de arquivos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
    "Personalizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
    "Vermelho Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
    "Laranja Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
    "Amarelo Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
    "Verde Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
    "Azul Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
    "Violeta Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
    "Cinza Clássico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
    "Vermelho Legado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
    "Roxo Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
    "Azul Meia Noite"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
    "Dourado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
    "Azul Elétrico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
    "Verde Maçã"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
    "Vermelho Vulcânico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
    "Lagoa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
    "Programador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
    "Drácula"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
    "Fio De Fada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
    "IU Plana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
    "Gruvbox Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
    "Gruvbox Claro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
    "Hackeando o Kernel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
    "Nódico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA,
    "Nova"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
    "Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
    "Noite Pálida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
    "Escuro Solarizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
    "Claro Solarizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
    "Escuro Tango"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
    "Claro Tango"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN,
    "Fogo Zen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN,
    "Anti-Fogo Zen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TRUE,
    "Verdadeiro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
    "Habilitar Companheiro da Interface de Usuário"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
    "Companheiro da Interface de Usuário Roda na Inicialização"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
    "Mostrar menu desktop na inicialização"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
    "Habilitar menu desktop (reiniciar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
    "Barra de Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
    "Incapaz de ler o arquivo comprimido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
    "Desfazer Carregamento de Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
    "Desfazer Salvamento de Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNKNOWN,
    "Desconhecido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
    "Atualizador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
    "Atualizar Recursos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
    "Atualizar Perfis de Auto Configuração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
    "Atualizar Shaders Cg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
    "Atualizar Trapaças"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
    "Atualizar Arquivos de Informação de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
    "Atualizar Bases de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
    "Atualizar Shaders GLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
    "Atualizar Lakka"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
    "Atualizar Sobreposições"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
    "Atualizar Shaders Slang"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER,
    "Usuário"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_KEYBOARD,
    "Kbd"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
    "Interface de Usuário"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
    "Idioma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
    "Usuário"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
    "Utilizar o Visualizador de Imagem Integrado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
    "Utilizar o Reprodutor de Mídia Integrado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
    "<Utilizar este diretório>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
    "Permitir Rotação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
    "Configurar Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
    "Proporção de Tela Automática"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
    "Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
    "Inserção de Quadro Opaco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
    "Cortar Overscan (Recarregar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
    "Desativar Composição da Área de Trabalho"
    )
#if defined(_3DS)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
    "Tela Inferior do 3DS"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
    "Ativa a exibição de informações de status na tela inferior. Desative para aumentar a vida útil da bateria e melhorar o desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
    "Modo de Exibição do 3DS"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
    "Selecione entre os modos de exibição 3D e 2D. No modo '3D', os pixels são quadrados e um efeito de profundidade é aplicado ao visualizar o Menu Rápido. O modo '2D' oferece o melhor desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_3D,
    "3D"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D,
    "2D"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400x240,
    "2D (Efeito de Grade de Pixels)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800x240,
    "2D (Alta Resolução)"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
    "Filtro de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
    "Filtro de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
    "Filtro de tremulação de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
    "Habilitar Notificações na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
    "Fonte das Notificações na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
    "Tamanho da Notificação na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
    "Forçar Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
    "Forçar Desativação de sRGB FBO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
    "Atraso de Quadro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
    "Atraso no Auto-Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
    "Iniciar em Modo de Tela Cheia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
    "Gama de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
    "Usar Gravação da GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
    "Habilitar Captura de Tela da GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
    "Sincronia Rígida de GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
    "Quadros de Sincronia Rígida de GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Máximo de imagens na cadeia de troca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
    "Posição X da Notificação na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
    "Posição Y da Notificação na Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
    "Índice de Monitor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
    "Usar Gravação Pós-Filtro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
    "Taxa de Atualização Vertical"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
    "Taxa de Quadros Estimada da Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
    "Definir Taxa de Atualização Reportada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
    "Rotação de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
    "Orientação da Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
    "Escala em Janela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
    "Threads de Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
    "Escala em Inteiros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
    "Shader de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
    "Estágios de Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
    "Parâmetros de Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
    "Carregar Predefinição de Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
    "Salvar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
    "Salvar Predefinição de Shader Como"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
    "Salvar Predefinição Global"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Salvar Predefinição de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Salvar Predefinição do Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Salvar Predefinição de Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
    "Habilitar Contexto Compartilhado de Hardware"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
    "Filtragem Bilinear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
    "Habilitar Filtro por Software"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
    "Intervalo de Troca da Sincronização Vertical (V-Sync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
    "Vídeo Paralelizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
    "Reduzir Tremulação de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Altura Personalizada da Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Largura Personalizada da Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
    "Posição X Personalizada da Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
    "Posição Y Personalizada da Proporção de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
    "Definir Largura de Tela do VI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
    "Correção de Overscan (Superior)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
    "Ajuste o corte do overscan da exibição reduzindo o tamanho da imagem pelo número especificado de linhas de varredura (tiradas da parte superior da tela). Nota: pode introduzir artefatos de dimensionamento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
    "Correção de Overscan (Inferior)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
    "Ajuste o corte do overscan da exibição reduzindo o tamanho da imagem pelo número especificado de linhas de varredura (tiradas da parte inferior da tela). Nota: pode introduzir artefatos de dimensionamento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
    "Sincronização Vertical (V-Sync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
    "Modo Janela em Tela Cheia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
    "Largura da Janela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
    "Altura da Janela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
    "Largura em Tela Cheia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
    "Altura em Tela Cheia"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
    "Ativar o Esquema de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
    "Os esquemas de vídeo são usados para molduras e outros trabalhos artísticos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
    "Caminho do Esquema de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
    "Selecione um esquema de vídeo no navegador de arquivos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
    "Visualização selecionada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
    "Selecione uma visualização dentro do esquema carregado."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
    "Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
    "Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
    "Fator Alfa do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
    "Cor Vermelha da Fonte do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
    "Cor Verde da Fonte do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
    "Cor Azul da Fonte do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_FONT,
    "Fonte do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
    "Personalizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
    "FlatUI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
    "Monocromático"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
     "Monocromático Invertido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
    "Sistemático"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
    "NeoActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
    "Pixel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
    "RetroActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
    "Retrosystem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
    "Dot-Art"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
    "Automático"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
    "Automático invertido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
    "Tema de Cor do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
    "Verde Maçã"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
    "Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
    "Claro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
    "Azul da Manhã"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
    "Violeta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
    "Roxo Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
    "Azul Elétrico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
    "Dourado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
    "Vermelho Legado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
    "Azul Meia-noite"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
    "Natural"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
    "Submarino"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
    "Vermelho Vulcânico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
    "Canal de Shaders do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
    "Sombras dos Ícones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
    "Exibir Aba de Histórico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
    "Exibir Aba de Importação de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
    "Exibir Abas de Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
    "Exibir Aba de Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
    "Exibir Aba de Imagem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
    "Exibir Aba de Música"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
    "Exibir Aba de Configurações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
    "Exibir Aba de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
    "Exibir Aba de Jogo em Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
    "Esquema do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_THEME,
    "Tema de Ícones do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YES,
    "Sim"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
    "Predefinição de Shader"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
    "Compita para ganhar conquistas personalizadas em jogos clássicos.\n"
    "Para mais informações, visite http://retroachievements.org"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
    "Habilita ou desabilita conquistas não oficiais e/ou recursos beta para fins de teste."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Dobra a quantidade de pontos ganhos.\n"
    "Desativa estado de jogo, trapaças, rebobinamento, pausa e câmera lenta em todos os jogos.\n"
    "A alternância dessa configuração em tempo de execução reiniciará seu jogo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
    "Tabelas de classificação específicas do jogo.\n"
    "Não tem efeito se o modo Hardcore estiver desativado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
    "Habilita ou desabilita a exibição de insígnia na Lista de Conquistas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
    "Habilita ou desabilita detalhes das conquistas na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
    "Obtém automaticamente uma captura de tela quando uma conquista é acionada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
    "Altera os drivers utilizados pelo sistema."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
    "Altera as configurações de conquistas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_SETTINGS,
    "Altera as configurações de núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
    "Altera as configurações de gravação."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
    "Altera as configurações de sobreposição e sobreposição de teclado, e as configurações de notificação na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
    "Altera as configurações de Rebobinamento, Avanço Rápido e Câmera Lenta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
    "Altera as configurações de salvamento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
    "Altera as configurações de registro de eventos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
    "Altera as configurações da interface de usuário."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_SETTINGS,
    "Altera as configurações de conta, nome de usuário e idioma."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
    "Altera as configurações de privacidade."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
    "Altera as configurações de MIDI."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
    "Altera os diretórios padrões onde os arquivos estão localizados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
    "Altera as configurações de lista de reprodução."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
    "Ajusta as configurações de servidor e rede."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
    "Analisa o conteúdo e adiciona na base de dados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
    "Altera as configurações de saída de áudio."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
    "Habilita ou desabilita o bluetooth."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
    "Salva as alterações nos arquivos de configuração ao sair."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
    "Altera as definições padrões para os arquivos de configuração."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
    "Gerencia e cria arquivos de configuração."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CPU_CORES,
    "Quantidade de Cores que a CPU possui."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FPS_SHOW,
    "Exibe a taxa atual de quadros por segundo na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
    "Exibe a contagem atual de quadros na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MEMORY_SHOW,
    "Inclui o uso de memória atual/total na tela com FPS/Quadros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
    "Ajusta as configurações das teclas de atalho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Combinação de botões do Gamepad para alternar o menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
    "Altera as configurações de Joypad, Teclado e Mouse."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_USER_BINDS, /* TODO/FIXME - change user to port */
    "Configura os controles para este usuário."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
    "Altera as configurações relacionadas a vídeo, áudio e latência dos comandos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
    "Habilita ou desabilita o registro de eventos no terminal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY,
    "Entrar ou hospedar uma sessão de jogo em rede."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
    "Procura e conecta aos anfitriões de jogo em rede na rede local."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
    "Exibe informações de núcleo, rede e sistema."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
    "Baixa complementos, componentes e conteúdo para o RetroArch."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
    "Habilita ou desabilita o compartilhamento de pastas na rede."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
    "Gerencia serviços ao nível de sistema operacional."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
    "Exibe arquivos/diretórios ocultos no navegador de arquivos."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_SSH_ENABLE,
    "Habilita ou desabilita o acesso remoto à linha de comando."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
    "Evita que a proteção de tela do seu sistema seja ativada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
    "Define o tamanho da janela em relação ao tamanho da janela de exibição do núcleo. Como alternativa, você pode definir uma largura e altura de janela abaixo para um tamanho de janela fixo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_LANGUAGE,
    "Define o idioma da interface."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
    "Insere um quadro opaco entre os quadros. Útil para usuários com telas de 120Hz que desejam jogar conteúdos em 60Hz para eliminar efeito de fantasma."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
    "Reduz a latência ao custo de maior risco de engasgamento de vídeo. Adiciona um atraso após o V-Sync (em ms)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
    "Atrasa o carregamento automático de shaders (em ms). Pode solucionar problemas gráficos ao usar o software 'captura de tela'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
    "Define quantos quadros a CPU pode rodar à frente da GPU quando utilizado o recurso 'Sincronia Rígida de GPU'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Informa ao driver de vídeo para utilizar explicitamente um modo de buffer específico."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
    "Seleciona qual tela de exibição a ser usada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
    "A taxa de atualização estimada da tela em Hz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
	"A taxa de atualização conforme relatada pelo driver de vídeo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
    "Altera as configurações de saída de vídeo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
    "Analisa redes sem fio e estabelecer uma conexão."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_LIST,
    "Saiba mais sobre como o programa funciona."
    )
MSG_HASH(
    MSG_ADDED_TO_FAVORITES,
    "Adicionado aos favoritos"
    )
MSG_HASH(
    MSG_ADD_TO_FAVORITES_FAILED,
    "Falha ao adicionar favorito: lista de reprodução cheia"
    )
MSG_HASH(
    MSG_SET_CORE_ASSOCIATION,
    "Conjunto de núcleo: "
    )
MSG_HASH(
    MSG_RESET_CORE_ASSOCIATION,
    "A associação do núcleo de entrada da lista de reprodução foi redefinida."
    )
MSG_HASH(
    MSG_APPENDED_DISK,
    "Disco anexado"
    )
MSG_HASH(
    MSG_APPLICATION_DIR,
    "Diretório do aplicativo"
    )
MSG_HASH(
    MSG_APPLYING_CHEAT,
    "Aplicando as alterações de Trapaças."
    )
MSG_HASH(
    MSG_APPLYING_SHADER,
    "Aplicando shader"
    )
MSG_HASH(
    MSG_AUDIO_MUTED,
    "Sem áudio."
    )
MSG_HASH(
    MSG_AUDIO_UNMUTED,
    "Áudio normal."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_ERROR_SAVING,
    "Erro em salvar o arquivo de auto configuração."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
    "Arquivo de autoconfiguração salvo com sucesso."
    )
MSG_HASH(
    MSG_AUTOSAVE_FAILED,
    "Não foi possível inicializar o salvamento automático."
    )
MSG_HASH(
    MSG_AUTO_SAVE_STATE_TO,
    "Salvar automaticamente Estado de Jogo em"
    )
MSG_HASH(
    MSG_BLOCKING_SRAM_OVERWRITE,
    "Bloqueando Sobrescrita da SRAM"
    )
MSG_HASH(
    MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
    "Trazendo a interface de comando na porta"
    )
MSG_HASH(
    MSG_BYTES,
    "bytes"
    )
MSG_HASH(
    MSG_CANNOT_INFER_NEW_CONFIG_PATH,
    "Não é possível inferir o novo caminho de configuração. Use a hora atual."
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Modo Hardcore Habilitado, Estados de Jogo e Rebobinamento estão desabilitados."
    )
MSG_HASH(
    MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
    "Comparando com números mágicos conhecidos..."
    )
MSG_HASH(
    MSG_COMPILED_AGAINST_API,
    "Compilado contra a API"
    )
MSG_HASH(
    MSG_CONFIG_DIRECTORY_NOT_SET,
    "Diretório de configuração não definido. Não foi possível salvar a nova configuração."
    )
MSG_HASH(
    MSG_CONNECTED_TO,
    "Conectado a"
    )
MSG_HASH(
    MSG_CONTENT_CRC32S_DIFFER,
    "O CRC32 dos conteúdos difere. Não é possível utilizar jogos diferentes."
    )
MSG_HASH(
    MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
    "Carregamento de conteúdo ignorado. A implementação irá carregar por conta própria."
    )
MSG_HASH(
    MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "O núcleo não suporta Estados de Jogo."
    )
MSG_HASH(
    MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
    "O arquivo de opções de núcleo foi criado com sucesso."
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
    "Não foi possível encontrar nenhum driver seguinte"
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
    "Não foi possível encontrar um sistema compatível."
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
    "Não foi possível encontrar uma faixa de dados válida"
    )
MSG_HASH(
    MSG_COULD_NOT_OPEN_DATA_TRACK,
    "Não foi possível abrir a faixa de dados"
    )
MSG_HASH(
    MSG_COULD_NOT_READ_CONTENT_FILE,
    "Não foi possível ler o arquivo de conteúdo"
    )
MSG_HASH(
    MSG_COULD_NOT_READ_MOVIE_HEADER,
    "Não foi possível ler o cabeçalho do filme."
    )
MSG_HASH(
    MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
    "Não foi possível ler o Estado de Jogo do filme."
    )
MSG_HASH(
    MSG_CRC32_CHECKSUM_MISMATCH,
    "Soma de verificação CRC32 incompatível entre o arquivo de conteúdo e a soma de verificação de conteúdo salva no cabeçalho do arquivo de reprodução. Reprodução altamente susceptível de dessincronizar na reprodução."
    )
MSG_HASH(
    MSG_CUSTOM_TIMING_GIVEN,
    "Tempo personalizado fornecido"
    )
MSG_HASH(
    MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
    "Descompressão já está em andamento."
    )
MSG_HASH(
    MSG_DECOMPRESSION_FAILED,
    "Descompressão falhou."
    )
MSG_HASH(
    MSG_DETECTED_VIEWPORT_OF,
    "Detectada janela de exibição de"
    )
MSG_HASH(
    MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
    "Não encontrou uma modificação de conteúdo válido."
    )
MSG_HASH(
    MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
    "Desconectar dispositivo de uma porta válida."
    )
MSG_HASH(
    MSG_DISK_CLOSED,
    "Fechado"
    )
MSG_HASH(
    MSG_DISK_EJECTED,
    "Ejetado"
    )
MSG_HASH(
    MSG_DOWNLOADING,
    "Baixando"
    )
MSG_HASH(
    MSG_INDEX_FILE,
    "index"
    )
MSG_HASH(
    MSG_DOWNLOAD_FAILED,
    "Download falhou"
    )
MSG_HASH(
    MSG_ERROR,
    "Erro"
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
    "O núcleo Libretro requer conteúdo, mas nada foi fornecido."
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
    "O núcleo Libretro requer conteúdo especial, mas nenhum foi fornecido."
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
    "O núcleo não suporta VFS, e o carregamento de uma cópia local falhou"
)
MSG_HASH(
    MSG_ERROR_PARSING_ARGUMENTS,
    "Erro em analisar os argumentos."
    )
MSG_HASH(
    MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
    "Erro em salvar o arquivo de opções de núcleo."
    )
MSG_HASH(
    MSG_ERROR_SAVING_REMAP_FILE,
    "Erro em salvar o arquivo de remapeamento."
    )
MSG_HASH(
    MSG_ERROR_REMOVING_REMAP_FILE,
    "Erro em remover o arquivo de remapeamento."
    )
MSG_HASH(
    MSG_ERROR_SAVING_SHADER_PRESET,
    "Erro em salvar a predefinição de Shader."
    )
MSG_HASH(
    MSG_EXTERNAL_APPLICATION_DIR,
    "Diretório de Aplicativo Externo"
    )
MSG_HASH(
    MSG_EXTRACTING,
    "Extraindo"
    )
MSG_HASH(
    MSG_EXTRACTING_FILE,
    "Extraindo arquivo"
    )
MSG_HASH(
    MSG_FAILED_SAVING_CONFIG_TO,
    "Falha em salvar a configuração em"
    )
MSG_HASH(
    MSG_FAILED_TO,
    "Falha em"
    )
MSG_HASH(
    MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
    "Falha em aceitar o espectador ingresso."
    )
MSG_HASH(
    MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
    "Falha em alocar memória para o conteúdo modificado..."
    )
MSG_HASH(
    MSG_FAILED_TO_APPLY_SHADER,
    "Falha em aplicar o Shader."
    )
MSG_HASH(
    MSG_FAILED_TO_APPLY_SHADER_PRESET,
    "Falha ao aplicar a predefinição de shader:"
    )
MSG_HASH(
    MSG_FAILED_TO_BIND_SOCKET,
    "Falha em vincular o soquete."
    )
MSG_HASH(
    MSG_FAILED_TO_CREATE_THE_DIRECTORY,
    "Falha em criar o diretório."
    )
MSG_HASH(
    MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
    "Falha em extrair o conteúdo do arquivo comprimido"
    )
MSG_HASH(
    MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
    "Falha em obter o apelido do cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD,
    "Falha em carregar"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_CONTENT,
    "Falha em carregar o conteúdo"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_MOVIE_FILE,
    "Falha em carregar o arquivo de filme"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_OVERLAY,
    "Falha em carregar a Sobreposição."
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_STATE,
    "Falha em carregar o Estado de Jogo de"
    )
MSG_HASH(
    MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
    "Falha em abrir o núcleo Libretro"
    )
MSG_HASH(
    MSG_FAILED_TO_PATCH,
    "Falha em executar a modificação"
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
    "Falha em receber o cabeçalho do cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME,
    "Falha em receber o apelido."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
    "Falha em receber o apelido do anfitrião."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
    "Falha em receber o tamanho do apelido do anfitrião."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
    "Falha em receber os dados SRAM do anfitrião."
    )
MSG_HASH(
    MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
    "Falha em remover o disco da bandeja."
    )
MSG_HASH(
    MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
    "Falha em remover o arquivo temporário"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_SRAM,
    "Falha em salvar SRAM"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_STATE_TO,
    "Falha em salvar o Estado de Jogo em"
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME,
    "Falha em enviar o apelido."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_SIZE,
    "Falha em enviar o tamanho do apelido."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
    "Falha em enviar o apelido para o cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
    "Falha em enviar o apelido para o anfitrião."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
    "Falha em enviar os dados SRAM para o cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_START_AUDIO_DRIVER,
    "Falha em iniciar o driver de áudio. Prosseguindo sem áudio."
    )
MSG_HASH(
    MSG_FAILED_TO_START_MOVIE_RECORD,
    "Falha em iniciar a gravação do filme."
    )
MSG_HASH(
    MSG_FAILED_TO_START_RECORDING,
    "Falha em iniciar a gravação."
    )
MSG_HASH(
    MSG_FAILED_TO_TAKE_SCREENSHOT,
    "Falha em obter uma captura de tela."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_LOAD_STATE,
    "Falha em desfazer o carregamento de Estado de Jogo."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_SAVE_STATE,
    "Falha em desfazer o salvamento de Estado de Jogo."
    )
MSG_HASH(
    MSG_FAILED_TO_UNMUTE_AUDIO,
    "Falha em desativar o áudio mudo."
    )
MSG_HASH(
    MSG_FATAL_ERROR_RECEIVED_IN,
    "Erro fatal recebido em"
    )
MSG_HASH(
    MSG_FILE_NOT_FOUND,
    "Arquivo não encontrado"
    )
MSG_HASH(
    MSG_FOUND_AUTO_SAVESTATE_IN,
    "Estado de Jogo automático encontrado em"
    )
MSG_HASH(
    MSG_FOUND_DISK_LABEL,
    "Rótulo de disco encontrado"
    )
MSG_HASH(
    MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
    "Encontrada primeira faixa de dados no arquivo"
    )
MSG_HASH(
    MSG_FOUND_LAST_STATE_SLOT,
    "Encontrada último compartimento de Estado de Jogo"
    )
MSG_HASH(
    MSG_FOUND_SHADER,
    "Shader encontrado"
    )
MSG_HASH(
    MSG_FRAMES,
    "Quadros"
    )
MSG_HASH(
    MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
    "Opções por Jogo: Opções de núcleo específicas do jogo encontradas em"
    )
MSG_HASH(
    MSG_GOT_INVALID_DISK_INDEX,
    "Índice de disco inválido obtido"
    )
MSG_HASH(
    MSG_GRAB_MOUSE_STATE,
    "Capturar estado do Mouse"
    )
MSG_HASH(
    MSG_GAME_FOCUS_ON,
    "Foco do jogo ligado"
    )
MSG_HASH(
    MSG_GAME_FOCUS_OFF,
    "Foco do jogo desligado"
    )
MSG_HASH(
    MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
    "O núcleo Libretro é renderizado por hardware. Deve usar a gravação pós-Shader também."
    )
MSG_HASH(
    MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
    "A soma de verificação inflada não corresponde ao CRC32."
    )
MSG_HASH(
    MSG_INPUT_CHEAT,
    "Entrada de Trapaça"
    )
MSG_HASH(
    MSG_INPUT_CHEAT_FILENAME,
    "Nome do Arquivo de Trapaça"
    )
MSG_HASH(
    MSG_INPUT_PRESET_FILENAME,
    "Nome de Arquivo de Predefinição"
    )
MSG_HASH(
    MSG_INPUT_RENAME_ENTRY,
    "Renomear Título"
    )
MSG_HASH(
    MSG_INTERFACE,
    "Interface"
    )
MSG_HASH(
    MSG_INTERNAL_STORAGE,
    "Armazenamento Interno"
    )
MSG_HASH(
    MSG_REMOVABLE_STORAGE,
    "Armazenamento Removível"
    )
MSG_HASH(
    MSG_INVALID_NICKNAME_SIZE,
    "Tamanho de apelido inválido."
    )
MSG_HASH(
    MSG_IN_BYTES,
    "em bytes"
    )
MSG_HASH(
    MSG_IN_GIGABYTES,
    "em gigabytes"
    )
MSG_HASH(
    MSG_IN_MEGABYTES,
    "em megabytes"
    )
MSG_HASH(
    MSG_LIBRETRO_ABI_BREAK,
    "foi compilado contra uma versão diferente do libretro do que esta."
    )
MSG_HASH(
    MSG_LIBRETRO_FRONTEND,
    "Frontend para Libretro"
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT,
    "Estado de Jogo carregado do compartimento #%d."
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT_AUTO,
    "Estado de Jogo carregado do compartimento #-1 (automático)."
    )
MSG_HASH(
    MSG_LOADING,
    "Carregando"
    )
MSG_HASH(
    MSG_FIRMWARE,
    "Um ou mais arquivos de firmware estão faltando"
    )
MSG_HASH(
    MSG_LOADING_CONTENT_FILE,
    "Carregando arquivo de conteúdo"
    )
MSG_HASH(
    MSG_LOADING_HISTORY_FILE,
    "Carregando arquivo de histórico"
    )
MSG_HASH(
    MSG_LOADING_FAVORITES_FILE,
    "Carregando o arquivo de favoritos"
    )
MSG_HASH(
    MSG_LOADING_STATE,
    "Carregando Estado de Jogo"
    )
MSG_HASH(
    MSG_MEMORY,
    "Memória"
    )
MSG_HASH(
    MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
    "O arquivo de filme não é um arquivo BSV1 válido."
    )
MSG_HASH(
    MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
    "O formato de filme parece ter uma versão de serializador diferente. Provavelmente irá falhar."
    )
MSG_HASH(
    MSG_MOVIE_PLAYBACK_ENDED,
    "Reprodução de filme terminou."
    )
MSG_HASH(
    MSG_MOVIE_RECORD_STOPPED,
    "Parando a gravação de filme."
    )
MSG_HASH(
    MSG_NETPLAY_FAILED,
    "Falha em inicializar o jogo em rede."
    )
MSG_HASH(
    MSG_NO_CONTENT_STARTING_DUMMY_CORE,
    "Sem conteúdo, iniciando um núcleo vazio."
    )
MSG_HASH(
    MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
    "Nenhum Estado de Jogo foi sobrescrito até o momento."
    )
MSG_HASH(
    MSG_NO_STATE_HAS_BEEN_LOADED_YET,
    "Nenhum Estado de Jogo foi carregado até o momento."
    )
MSG_HASH(
    MSG_OVERRIDES_ERROR_SAVING,
    "Erro em salvar as redefinições."
    )
MSG_HASH(
    MSG_OVERRIDES_SAVED_SUCCESSFULLY,
    "Redefinições salvas com sucesso."
    )
MSG_HASH(
    MSG_PAUSED,
    "Pausado."
    )
MSG_HASH(
    MSG_PROGRAM,
    "RetroArch"
    )
MSG_HASH(
    MSG_READING_FIRST_DATA_TRACK,
    "Lendo a primeira faixa de dados..."
    )
MSG_HASH(
    MSG_RECEIVED,
    "recebido"
    )
MSG_HASH(
    MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
    "A gravação terminou devido ao redimensionamento."
    )
MSG_HASH(
    MSG_RECORDING_TO,
    "Gravando em"
    )
MSG_HASH(
    MSG_REDIRECTING_CHEATFILE_TO,
    "Redirecionando o arquivo de Trapaça em"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVEFILE_TO,
    "Redirecionando o Jogo-Salvo em"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVESTATE_TO,
    "Redirecionando o Estado de Jogo em"
    )
MSG_HASH(
    MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
    "Arquivo de remapeamento salvo com sucesso."
    )
MSG_HASH(
    MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
    "Arquivo de remapeamento salvo com sucesso."
    )
MSG_HASH(
    MSG_REMOVED_DISK_FROM_TRAY,
    "Disco removido da bandeja."
    )
MSG_HASH(
    MSG_REMOVING_TEMPORARY_CONTENT_FILE,
    "Removendo arquivo de conteúdo temporário"
    )
MSG_HASH(
    MSG_RESET,
    "Reinicializar"
    )
MSG_HASH(
    MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
    "Reiniciando a gravação devido ao reinício do driver."
    )
MSG_HASH(
    MSG_RESTORED_OLD_SAVE_STATE,
    "Estado de Jogo antigo restaurado."
    )
MSG_HASH(
    MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
    "Shaders: restaurando predefinição padrão de Shader em"
    )
MSG_HASH(
    MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
    "Revertendo diretório de Jogo-Salvo em"
    )
MSG_HASH(
    MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
    "Revertendo diretório de Estado de Jogo em"
    )
MSG_HASH(
    MSG_REWINDING,
    "Rebobinando."
    )
MSG_HASH(
    MSG_REWIND_INIT,
    "Inicializando o buffer de Rebobinamento com tamanho"
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED,
    "Falha em inicializar o buffer de Rebobinamento. O rebobinamento será desativado."
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
    "Esta implementação usa áudio paralelizado. Não é possível utilizar o rebobinamento."
    )
MSG_HASH(
    MSG_REWIND_REACHED_END,
    "Final do buffer de rebobinamento atingido."
    )
MSG_HASH(
    MSG_SAVED_NEW_CONFIG_TO,
    "Nova configuração salva em"
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT,
    "Estado de Jogo salvo no compartimento #%d."
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT_AUTO,
    "Estado de Jogo salvo no compartimento #-1 (automático)."
    )
MSG_HASH(
    MSG_SAVED_SUCCESSFULLY_TO,
    "Salvo com sucesso em"
    )
MSG_HASH(
    MSG_SAVING_RAM_TYPE,
    "Salvando Tipo de RAM"
    )
MSG_HASH(
    MSG_SAVING_STATE,
    "Salvando Estado de Jogo"
    )
MSG_HASH(
    MSG_SCANNING,
    "Analisando"
    )
MSG_HASH(
    MSG_SCANNING_OF_DIRECTORY_FINISHED,
    "Análise de diretório terminada"
    )
MSG_HASH(
    MSG_SENDING_COMMAND,
    "Enviando comando"
    )
MSG_HASH(
    MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
    "Várias modificações de conteúdo estão explicitamente definidas, ignorando todas..."
    )
MSG_HASH(
    MSG_SHADER,
    "Shader"
    )
MSG_HASH(
    MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
    "Predefinição de Shader salva com sucesso."
    )
MSG_HASH(
    MSG_SKIPPING_SRAM_LOAD,
    "Ignorando carregamento da SRAM."
    )
MSG_HASH(
    MSG_SLOW_MOTION,
    "Câmera Lenta."
    )
MSG_HASH(
    MSG_FAST_FORWARD,
     "Avanço rápido."
    )
MSG_HASH(
    MSG_SLOW_MOTION_REWIND,
    "Rebobinamento em Câmera Lenta."
    )
MSG_HASH(
    MSG_SRAM_WILL_NOT_BE_SAVED,
    "SRAM não será salva."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_PLAYBACK,
    "Iniciando reprodução de filme."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_RECORD_TO,
    "Iniciando a gravação de filme em"
    )
MSG_HASH(
    MSG_STATE_SIZE,
    "Tamanho do Estado de Jogo"
    )
MSG_HASH(
    MSG_STATE_SLOT,
    "Compartimento do Estado de Jogo"
    )
MSG_HASH(
    MSG_TAKING_SCREENSHOT,
    "Obtendo captura de tela."
    )
MSG_HASH(
    MSG_SCREENSHOT_SAVED,
    "Captura de tela salva"
    )
MSG_HASH(
    MSG_ACHIEVEMENT_UNLOCKED,
   "Conquista desbloqueada"
    )
MSG_HASH(
    MSG_CHANGE_THUMBNAIL_TYPE,
    "Alterar o tipo de miniatura"
    )
MSG_HASH(
    MSG_NO_THUMBNAIL_AVAILABLE,
    "Nenhuma miniatura disponível"
    )
MSG_HASH(
    MSG_PRESS_AGAIN_TO_QUIT,
    "Pressione novamente para sair..."
    )
MSG_HASH(
    MSG_TO,
    "em"
    )
MSG_HASH(
    MSG_UNDID_LOAD_STATE,
    "Desfez o carregamento de Estado de Jogo."
    )
MSG_HASH(
    MSG_UNDOING_SAVE_STATE,
    "Desfazendo o salvamento de Estado de Jogo"
    )
MSG_HASH(
    MSG_UNKNOWN,
    "Desconhecido"
    )
MSG_HASH(
    MSG_UNPAUSED,
    "Retomando."
    )
MSG_HASH(
    MSG_UNRECOGNIZED_COMMAND,
    "Comando não reconhecido"
    )
MSG_HASH(
    MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
    "Usando o nome do núcleo para uma nova configuração."
    )
MSG_HASH(
    MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
    "Usando o núcleo vazio. Pulando a gravação."
    )
MSG_HASH(
    MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
    "Conecte o dispositivo a partir de uma porta válida."
    )
MSG_HASH(
    MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
    "Desconectando o dispositivo da porta"
    )
MSG_HASH(
    MSG_VALUE_REBOOTING,
    "Reinicializando..."
    )
MSG_HASH(
    MSG_VALUE_SHUTTING_DOWN,
    "Desligando..."
    )
MSG_HASH(
    MSG_VERSION_OF_LIBRETRO_API,
    "Versão da API libretro"
    )
MSG_HASH(
    MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
    "Falha no cálculo de tamanho da janela de exibição! Prosseguindo usando dados brutos. Isto provavelmente não funcionará corretamente..."
    )
MSG_HASH(
    MSG_VIRTUAL_DISK_TRAY,
    "bandeja de disco virtual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
    "Latência de áudio desejada em milissegundos. Pode não ser honrado se o driver de áudio não puder prover a latência desejada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MUTE,
    "Silencia o áudio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
    "Ajuda a suavizar as imperfeições na regulagem ao sincronizar áudio e vídeo. Esteja ciente que se desativado, será quase impossível de se obter a sincronia adequada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
    "Permite ou não o acesso à câmera pelos núcleos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
    "Permite ou não o acesso ao serviço de localização pelos núcleos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
    "Número máximo de usuários suportados pelo RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
    "Influencia como a chamada seletiva de entrada é feita dentro do RetroArch. Definindo com 'Cedo' ou 'Tarde' pode resultar em menos latência, dependendo da sua configuração."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
    "Permite a qualquer usuário controlar o menu. Se desabilitado, apenas o Usuário 1 poderá controlar o menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
    "Volume do áudio (em dB). 0dB é o volume normal, e nenhum ganho é aplicado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "Permite ao driver WASAPI obter controle exclusivo do dispositivo de áudio. Se desativado, o modo compartilhado será utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
    "Utiliza formato de ponto flutuante para o driver WASAPI, se suportado pelo dispositivo de áudio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "O tamanho (em quadros) do buffer intermediário quando o driver WASAPI estiver em modo compartilhado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SYNC,
    "Sincroniza o áudio. Recomendado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
    "Até que ponto um eixo deve ser movido para resultar em um botão pressionado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
    "Quantidade de segundos para aguardar até proceder para o próximo vínculo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
    "Quantidade de segundos para manter uma entrada para vinculá-la."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
    "Descreve o período quando os botões com turbo habilitado são alternados. Os números são descritos em quadros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
    "Descreve quão longo deve ser o período de um botão com turbo habilitado. Os números são descritos como quadros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
    "Sincroniza o vídeo de saída da placa gráfica com a taxa de atualização da tela. Recomendado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
    "Permite que os núcleos definam a rotação. Quando desabilitado, as requisições de rotação são ignoradas. Útil para configurações onde se rotaciona manualmente a tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
    "Alguns núcleos podem ter um recurso de desligamento. Se habilitado, impedirá que o núcleo feche o RetroArch. Em vez disto, carrega um núcleo vazio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
    "Verifica se todos os firmwares necessários estão presentes antes de tentar carregar conteúdo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
    "Taxa de atualização vertical da sua tela. Utilizado para calcular uma taxa de saída de áudio adequada.\n"
    "OBS: Isto será ignorado se a função 'Vídeo Paralelizado' estiver habilitada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
    "Habilita a saída de áudio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
    "Mudança máxima na taxa de entrada de áudio. Se aumentado habilita grandes mudanças na regulagem ao custo de imprecisão no timbre do som (ex: rodando núcleos PAL em modo NTSC)."
    )
MSG_HASH(
    MSG_FAILED,
    "falhou"
    )
MSG_HASH(
    MSG_SUCCEEDED,
    "teve êxito"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED,
    "não configurado"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
    "não configurado, usando reserva"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
    "Lista de Cursores da Base de Dados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
    "Base de Dados - Filtro : Desenvolvedor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
    "Base de Dados - Filtro : Publicador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISABLED,
    "Desabilitado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLED,
    "Habilitado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
    "Caminho do Histórico de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
    "Base de Dados - Filtro : Origem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
    "Base de Dados - Filtro : Franquia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
    "Base de Dados - Filtro : Classificação ESRB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
    "Base de Dados - Filtro : Classificação ELSPA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
    "Base de Dados - Filtro : Classificação PEGI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
    "Base de Dados - Filtro : Classificação CERO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
    "Base de Dados - Filtro : Classificação BBFC"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
    "Base de Dados - Filtro : Usuários máximos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
    "Base de Dados - Filtro : Data de Lançamento Por Mês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
    "Base de Dados - Filtro : Data de Lançamento Por Ano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Base de Dados - Filtro : Edição da Revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
    "Base de Dados - Filtro : Classificação da Revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
    "Informações da Base de Dados"
    )
MSG_HASH(
    MSG_WIFI_SCAN_COMPLETE,
    "Análise de Wi-Fi completa."
    )
MSG_HASH(
    MSG_SCANNING_WIRELESS_NETWORKS,
    "Analisando redes sem fio..."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCAN_COMPLETE,
    "Análise de jogo em rede completa."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCANNING,
    "Analisando por anfitriões de jogo em rede..."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
    "Pausa o jogo quando a janela do RetroArch não está ativa."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
    "Ativa ou desativa a composição (Somente no Windows)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
    "Habilita ou desabilita a lista de reprodução recente para jogos, imagens, música e vídeos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
    "Limita o número de itens da lista de reprodução recente para jogos, imagens, música e vídeos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
    "Controles de Menu Unificados"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
    "Utiliza os mesmos controles para o menu e jogo. Aplica-se ao teclado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
    "Pressione Sair Duas Vezes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
    "Pressione a tecla de atalho Sair duas vezes para sair do RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
    "Exibe as mensagens na tela."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
    "Habilitar Remoto do Usuário %d"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
    "Exibir nível de bateria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
    "Mostrar sub-etiquetas do menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
    "Mostra informações adicionais para a entrada de menu atualmente selecionada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SELECT_FILE,
    "Selecionar Arquivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
    "Selecione de uma lista de reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER,
    "Filtro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCALE,
    "Escala"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
    "O jogo em rede irá iniciar quando o conteúdo for carregado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
    "Não foi possível encontrar um núcleo adequado ou arquivo de conteúdo, carregue manualmente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
    "Navegar pela URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL,
    "Caminho da URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_START,
    "Iniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
    "Bokeh"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
    "Floco de neve"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
    "Atualizar Lista de Salas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
    "Apelido: %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
    "Apelido (lan): %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
    "Conteúdo compatível encontrado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
    "Corta alguns pixels ao redor das bordas da imagem habitualmente deixada em branco por desenvolvedores, que por vezes também contêm pixels de lixo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
    "Adiciona um leve embaciado à imagem para suavizar as arestas da borda dos pixels. Esta opção tem pouco impacto no desempenho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER,
    "Aplica um filtro de vídeo processado pela CPU.\n"
    "OBS: Pode vir a um alto custo de desempenho. Alguns filtros de vídeo podem funcionar apenas para núcleos que usam cores de 32 bits ou 16 bits."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
    "Insira o nome de usuário de sua conta RetroAchievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
    "Insira a senha de sua conta RetroAchievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
    "Insira seu nome de usuário aqui. Isto será utilizado para sessões do jogo em rede, entre outras coisas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
    "Captura a imagem depois que os filtros (mas não os Shaders) forem aplicados. Seu vídeo ficará tão bonito quanto o que você vê na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_LIST,
    "Seleciona qual núcleo utilizar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_START_CORE,
    "Inicia o núcleo sem conteúdo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
    "Instala um núcleo do atualizador on-line."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
    "Instala ou restaura um núcleo do diretório de downloads."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
    "Seleciona qual conteúdo iniciar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
    "Exibe as interfaces de rede e endereços de IP associados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
    "Exibe informações específicas do dispositivo."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Reinicia o programa."
    )
#else
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Sai do programa."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
    "Reinicia o programa."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
    "Define a largura personalizada para a janela de exibição."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
    "Definir a altura personalizada para a janela de exibição."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
    "Lembre-se do tamanho e posição da janela, permitindo que isso tenha precedência sobre a Escala em Janela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
    "Define a largura personalizada para o modo de tela cheia em não-janela. Deixar desativado irá usar a resolução da área de trabalho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
    "Define a altura personalizada para o modo de tela cheia em não-janela. Deixar desativado irá usar a resolução da área de trabalho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
    "Especifique a posição personalizada no eixo X para o texto na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
    "Especifique a posição personalizada no eixo Y para o texto na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
    "Especifique o tamanho da fonte em pontos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
    "Oculta a Sobreposição enquanto estiver dentro do menu e exibe novamente ao sair."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Exibe comandos de teclado/controle na sobreposição."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Selecione a porta para a sobreposição escutar se Exibir Comandos na Sobreposição estiver habilitado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Mostra o cursor do mouse ao usar uma sobreposição na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
    "O conteúdo verificado correspondente ao banco de dados aparecerá aqui."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
    "Apenas dimensiona o vídeo em valores inteiros. O tamanho de base depende da geometria relatada pelo sistema e da proporção de tela. Se 'Forçar Proporção' não estiver definido, X / Y serão dimensionados independentemente em valores inteiros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
    "Captura a tela com Shader de GPU se disponível."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
    "Força uma certa rotação da tela. A rotação é adicionada a rotação que o núcleo definir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
    "Força uma certa orientação da tela do sistema operacional."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
    "Desabilita de forma forçada o suporte sRGB FBO. Alguns drivers Intel OpenGL no Windows possuem problemas de vídeo com o suporte sRGB FBO se estiver habilitado. Habilitando isto pode contornar o problema."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
    "Inicia em tela cheia. Pode ser mudado a qualquer momento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
    "Se estiver em tela cheia, prefira utilizar uma janela de tela cheia."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
    "Grava o material de saída do Shader de GPU se disponível."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
    "Ao criar um Estado de Jogo, o índice do Estado de Jogo é aumentado automaticamente antes de ser salvo. Ao carregar um conteúdo, o índice será definido para o índice mais alto existente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
    "Bloqueia a SRAM de ser sobrescrita ao carregar um Estado de Jogo. Pode causar problemas no jogo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
    "Taxa máxima em que o conteúdo será executado quando utilizado o Avanço Rápido (ex: 5.0x para conteúdos em 60fps = 300 fps máx). Se for definido como 0.0x, a taxa de Avanço Rápido é ilimitada (sem FPS máx)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
    "Quando está em Câmera Lenta, o conteúdo será diminuído pelo fator especificado/definido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
    "Executa o núcleo lógico um ou mais quadros à frente e carrega o estado de volta para reduzir o atraso dos controles percebido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
    "O número de quadros para avançar. Causa problemas de jogabilidade, como instabilidade, se você exceder o número de quadros de atraso internos do jogo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
    "O número de milissegundos a aguardar para obter uma amostra de entrada completa, use-a se você tiver problemas com pressionamentos de botão simultâneos (somente Android)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
    "Usa uma segunda instância do núcleo do RetroArch para avançar quadros. Evita problemas de áudio devido ao estado de carregamento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
    "Oculta a mensagem de aviso que aparece ao usar o Adiantar Quadro e o núcleo não suporta Estados de Jogo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_ENABLE,
    "Cometeu um erro? Rebobine e tente novamente.\n"
    "Cuidado pois isso causa um impacto no desempenho ao jogar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
    "Aplicar a trapaça imediatamente após alternância."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
    "Aplicar trapaças automaticamente quando o jogo for carregado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
    "O número de vezes que a trapaça será aplicada.\n"
    "Use com as outras duas opções de iteração para afetar grandes áreas da memória."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Após cada 'Número de Iterações', o Endereço de Memória será aumentado pelo número de vezes do 'Tamanho da Pesquisa da Memória'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
    "Após cada Número de Iterações, o Valor será aumentado por esse valor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
    "Ao definir um número de quadros para o rebobinamento, você pode retroceder vários quadros de uma só vez, aumentando a velocidade da função."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
    "A quantidade de memória (em MB) para reservar ao buffer de rebobinamento .  Aumentar esse valor aumentará a quantidade de histórico do rebobinamento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
    "Sempre que você aumentar ou diminuir o valor do tamanho do buffer do rebobinamento por meio dessa interface, ele será alterado por esse valor"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_IDX,
    "Posição do índice na lista."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
    "Bitmask o endereço quando o Tamanho da Busca da Memória para < 8-bit."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
    "Selecionar a coincidência para visualizar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
    "Esquerda/Direita para alterar o tamanho do bit"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
    "Esquerda/Direita para alterar o valor"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_NEQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
    "Esquerda/Direita para alterar o valor"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
    "Esquerda/Direita para alterar o valor"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_VIEW_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CREATE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
    "Big endian  : 258 = 0x0102,\n"
    "Little endian : 258 = 0x0201"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
    "Define o nível de registro de eventos para os núcleos. Se o nível do registro enviado por um núcleo for abaixo deste valor, o mesmo é ignorado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
    "Contadores de desempenho para o RetroArch (e núcleos).\n"
    "O contador de dados pode ajudar a determinar os gargalos do sistema e ajustar o desempenho do sistema e do aplicativo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
    "Cria automaticamente um Estado de Jogo no final da execução do RetroArch. O RetroArch irá carregar automaticamente este Estado de Jogo se a função 'Autocarregar Estado de Jogo' estiver habilitada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
    "Carrega automaticamente o último Estado de Jogo auto salvo na inicialização do RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
    "Mostrar miniaturas de estados salvos dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
    "Salva automaticamente o Save RAM não-volátil em um intervalo regular. Isso está desabilitado por padrão, a menos que seja definido de outra forma. O intervalo é medido em segundos. Um valor de 0 desativa o salvamento automático."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
    "Se ativado, substitui os vínculos de entrada com as associações remapeadas definidas para o núcleo atual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
    "Habilita a detecção automática de entrada. Tentará configurar automaticamente joypads, estilo Plug-and-Play."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
    "Troca de botões para OK/Cancelar. Desabilitado é o estilo de botão japonês, habilitada é o estilo ocidental."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
    "Se desabilitado, o conteúdo continuará sendo executado em segundo plano quando o menu do RetroArch for alternado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
    "Driver de vídeo a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
    "Driver de áudio a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DRIVER,
    "Driver de entrada a ser utilizado. Dependendo do driver de vídeo, pode forçar um driver de entrada diferente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
    "Driver do Joypad a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
    "Driver de reamostragem de áudio a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
    "Driver de câmera a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
    "Driver de localização a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DRIVER,
    "Driver de menu a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORD_DRIVER,
    "Driver de gravação a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_DRIVER,
    "Driver MIDI a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_DRIVER,
    "Driver de WiFi a ser utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Filtra os arquivos em exibição no explorador de arquivos por extensões suportadas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
    "Selecione uma imagem para definir como plano de fundo do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
    "Carrega dinamicamente um novo plano de fundo dependendo do contexto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
    "Substitui o dispositivo de áudio padrão utilizado pelo driver de áudio. Isto depende do driver."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
    "Plugin DSP de Áudio que processa o áudio antes de ser enviado para o driver."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
    "Taxa de amostragem da saída de áudio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
    "Opacidade de todos os elementos de interface da Sobreposição."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
    "Escala de todos os elementos de interface da Sobreposição."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
    "As Sobreposições são usadas para bordas e controles na tela"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
    "Selecione uma Sobreposição pelo navegador de arquivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
    "Endereço do anfitrião a se conectar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
    "Porta do endereço de IP do anfitrião. Pode ser uma porta TCP ou uma porta UDP."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
    "Senha para conectar ao anfitrião de jogo em rede. Utilizado apenas no modo anfitrião."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
    "Anuncia os jogos em rede publicamente. Se não for definido, os clientes deverão conectar manualmente em vez de usar o lobby público."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
    "Senha para conectar ao anfitrião de jogo em rede apenas com privilégios de espectador. Utilizado apenas no modo anfitrião."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
    "Define se o jogo em rede deve iniciar em modo espectador."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
    "Define se conexões em modo escravo são permitidas. Clientes em modo escravo requerem muito pouco poder de processamento em ambos os lados, mas irão sofrer significamente da latência de rede."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
    "Define se conexões que não estão em modo escravo são proibidas. Não recomendado, exceto para redes muito rápidas com máquinas muito lentas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
    "Define se deve executar o jogo em rede em modo que não utilize Estados de Jogo. Se definido como verdadeiro, uma rede muito rápida é necessária, mas nenhum rebobinamento é realizado, portanto, não haverá instabilidade no jogo em rede."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
    "Frequência em quadros no qual o jogo em rede verificará se o anfitrião e o cliente estão sincronizados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
    "Ao hospedar uma partida, tente receber conexões da Internet pública usando UPnP ou tecnologias similares para escapar das redes locais."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
    "Habilita a interface de comando stdin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
    "Habilita o controle por Mouse dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POINTER_ENABLE,
    "Habilita o controle por toque dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS,
    "Tipo de miniatura a ser exibida."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
    "Tipo de miniatura a ser exibido no canto superior direito das listas de reprodução. Esta miniatura pode ser alternada em tela cheia pressionando RetroPad Y."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
    "Tipo de miniatura para exibir à esquerda."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
    "Tipo de miniatura a ser exibido no canto inferior direito das listas de reprodução."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Substitui o painel de metadados do conteúdo por outra miniatura."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
    "Exibe a miniatura esquerda sob a direita, no lado direito da tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
    "Exibe data e/ou hora atuais dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
    "Exibe o nível de bateria atual dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
    "Volta ao início ou final se o limite da lista for alcançado horizontalmente ou verticalmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
    "Habilita o jogo em rede no modo anfitrião (servidor)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
    "Habilita o jogo em rede no modo cliente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
    "Desconecta de uma conexão de jogo em rede ativa."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
    "Analisa um diretório por arquivos compatíveis e os adiciona à coleção."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_FILE,
    "Analisa um arquivo compatível e o adiciona à coleção."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
    "Usa um intervalo de troca personalizado para V-Sync. Defina para reduzir efetivamente a taxa de atualização do monitor pela metade."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
    "Ordena os Jogos-Salvos em pastas com o nome do núcleo utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
    "Ordena os Estados de Jogo em pastas com o nome do núcleo utilizado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
    "Solicita jogar com o dispositivo de entrada dado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
    "URL para o diretório de atualização de núcleos no buildbot do Libreto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
    "URL para o diretório de atualizações de recursos no buildbot do Libretro."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "Após o download, extrair automaticamente os arquivos contidos nos arquivos comprimidos baixados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
    "Analisa novas salas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DELETE_ENTRY,
    "Remove esta entrada da coleção."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION,
    "Visualiza mais informações sobre o conteúdo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
    "Adiciona o item aos seus favoritos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
    "Adiciona o item aos seus favoritos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN,
    "Inicia o conteúdo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
    "Ajusta as definições do navegador de arquivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
    "Habilita por padrão controles personalizados na inicialização."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
    "Habilita por padrão configuração personalizada na inicialização."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
    "Habilita por padrão opções de núcleo personalizadas na inicialização."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ENABLE,
    "Exibe o nome do núcleo atual dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
    "Visualiza as bases de dados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
    "Visualiza as pesquisas anteriores."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
    "Captura uma imagem da tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
    "Fecha o conteúdo atual. Alterações não salvas serão perdidas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_STATE,
    "Carrega um Estado de Jogo do compartimento selecionado atualmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_STATE,
    "Salva um Estado de Jogo no compartimento selecionado atualmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME,
    "Continua a execução do conteúdo atual e sai do Menu Rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME_CONTENT,
    "Continua a execução do conteúdo atual e sai do Menu Rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATE_SLOT,
    "Altera o compartimento do Estado de Jogo selecionado atualmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
    "Se um Estado de Jogo for carregado, o conteúdo voltará ao estado anterior ao carregamento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
    "Se o Estado de Jogo foi sobrescrito, ele voltará ao Estado de Jogo salvo anteriormente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "Serviço RetroAchievements. Para mais informações, visite http://retroachievements.org (em inglês)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
    "Gerencia as contas configuradas atualmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
    "Gerencia as configurações do rebobinamento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
    "Gerencia as configurações dos detalhes da trapaça."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
    "Inicia ou continua uma pesquisa de código de trapaça."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_CONTENT,
    "Reinicia o conteúdo do começo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Salva um arquivo de redefinição de configuração que será aplicado a todo o conteúdo carregado por este núcleo. Terá prioridade sobre a configuração principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Salva um arquivo de redefinição de configuração que será aplicado a todo o conteúdo carregado no mesmo diretório que o arquivo atual. Terá prioridade sobre a configuração principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Salva um arquivo de redefinição de configuração que será aplicado apenas ao conteúdo atual. Terá prioridade sobre a configuração principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
    "Configura os códigos de Trapaça."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
    "Configura o Shader para realçar a aparência da imagem."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
    "Altera os controles para o conteúdo que está sendo executado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_OPTIONS,
    "Altera as opções para o conteúdo que está sendo executado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
    "Exibe as configurações avançadas para usuários experientes (oculto por padrão)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
    "Executar tarefas em linhas de processamento paralelas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
    "Permite que o usuário possa remover itens das coleções."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
    "Define o diretório de sistema. Os núcleos podem consultar este diretório para carregar arquivos de BIOS, configurações específicas do sistema, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
    "Define o diretório inicial do navegador de arquivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DIR,
    "Usualmente definido por desenvolvedores que agrupam aplicativos Libretro/RetroArch para apontar para os recursos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Diretório para armazenar planos de fundo dinamicamente carregados pelo menu dependendo do contexto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
    "Miniaturas auxiliares (arte da embalagem/imagens diversas e etc.) são armazenadas aqui."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
    "Define o diretório inicial para o navegador de configurações do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "O número de quadros de latência de entrada para o jogo em rede utilizar para mascarar a latência da rede. Reduz a oscilação e torna o jogo em rede menos intensivo para a CPU, ao custo de atraso perceptível na entrada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "O intervalo de quadros de latência de entrada que pode ser utilizado para mascarar a latência da rede. Reduz a oscilação e torna o jogo em rede menos intensivo para a CPU, ao custo de atraso imprevisível na entrada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_OPTIONS,
    "Gerenciamento de imagem de disco."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
    "Certifica-se de que a taxa de quadros é controlada enquanto estiver dentro do menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
    "Não desvia dos tempos solicitados pelo núcleo. Use com telas de Taxa de Atualização Variável, G-Sync, FreeSync."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_LAYOUT,
    "Selecione um esquema diferente para a interface XMB."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_THEME,
    "Selecione um tema diferente de ícone para o RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
    "Habilita as sombras para todos os ícones.\n"
    "Isto terá um pequeno impacto no desempenho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
    "Selecione um tema de gradiente de cor de plano de fundo diferente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
    "Modifica a opacidade do plano de fundo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
    "Selecione um tema de gradiente de cor de plano de fundo diferente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
    "Selecione um efeito de plano de fundo animado. Pode exigir mais processamento da GPU dependendo do efeito. Se o desempenho for insatisfatório, desligue este efeito ou reverta para um efeito mais simples."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_FONT,
    "Selecione uma fonte principal diferente para ser usada pelo menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
    "Exibe a aba de favoritos dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
    "Exibe a aba de imagem dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
    "Exibe a aba de música dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
    "Exibe a aba de vídeo dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
    "Exibe a aba de jogo em rede dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
    "Exibe a aba de configurações dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
    "Exibe a aba de histórico recente dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
    "Exibe a aba de importação de conteúdo dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
    "Exibe abas da lista de reprodução dentro do menu principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
    "Exibe a tela inicial no menu. É automaticamente definido como falso após o programa iniciar pela primeira vez."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
    "Modifica a opacidade do gráfico do cabeçalho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
    "Modifica a opacidade do gráfico do rodapé."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
    "Salva todos os arquivos baixados neste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
    "Salva todos os controles remapeados neste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
    "Diretório onde o programa busca por conteúdo/núcleos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
    "Os arquivos de informação do aplicativo/núcleo são armazenados aqui."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
    "Se um Joypad estiver conectado, o mesmo será configurado automaticamente se um arquivo de configuração correspondente estiver presente dento deste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
    "Salva todas as coleções neste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
    "Se for definido um diretório, o conteúdo que é temporariamente extraído (ex: dos arquivos) sera extraído para este diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
    "As consultas salvas são armazenadas neste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
    "As bases de dados são armazenadas neste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
    "Esta localização é consultada por padrão quando a interface do menu tenta procurar por recursos carregáveis, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
    "Salva todos os Jogos-Salvos neste diretório. Se não for definido, tentaremos salvar dentro do diretório de trabalho do arquivo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
    "Salva todos os Estados de Jogo neste diretório. Se não for definido, tentaremos salvar dentro do diretório de trabalho do arquivo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
    "Diretório para armazenar as capturas de tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
    "Define um diretório onde as sobreposições são mantidas para facilitar o acesso."
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
    "Define um diretório onde os esquemas de vídeo são mantidos para facilitar o acesso."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
    "Os arquivos de Trapaça são mantidos aqui."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
    "Diretório onde os arquivos de filtro DSP de áudio são mantidos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
    "Diretório onde os arquivos de filtro de vídeo processado por CPU são mantidos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
    "Define um diretório onde os arquivos de Shader de vídeo processado por GPU são mantidos para fácil acesso."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
    "As gravações serão armazenadas neste diretório."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
    "As configurações de gravação serão mantidas aqui."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
    "Selecione uma fonte diferente para as notificações na tela."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
    "As alterações das configurações de Shader terão efeito imediato. Use isto se você alterou a quantidade de estágios de Shader, filtros, escala FBO, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
    "Aumentar ou diminuir a quantidade de passadas do Shader. Você pode adicionar um Shader separado para cada estágio do pipeline e configurar sua escala e filtro."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
    "Carregar uma predefinição de Shader. Será definido automaticamente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
    "Salva a predefinição atual do shader."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
    "Salvar as definições de Shader atuais como uma nova predefinição de Shader."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Salvar as definições de Shader atuais como a definição padrão para esta aplicação/núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Salve as configurações atuais do shader como as configurações padrão para todos os arquivos no diretório de conteúdo atual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Salvar as definições de Shader atuais como a definição padrão para o conteúdo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
    "Salva as configurações atuais do shader como a configuração global padrão."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
    "Modifica diretamente o Shader atual. As alterações não serão salvas no arquivo de predefinição."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
    "Modifica a predefinição de Shader atualmente utilizada no menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
    "Aumentar ou diminuir a quantidade de Trapaças."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
    "As alterações de Trapaça terão efeito imediato."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
    "Iniciar a procura por uma nova trapaça. O número de bits pode ser alterado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
    "Continuar procurando por uma nova trapaça."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
    "Carregar um arquivo de Trapaça."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
    "Carregar um arquivo de trapaça e anexar às trapaças existentes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
    "Salvar as Trapaças atuais como um arquivo de Jogo-Salvo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
    "Acesse rapidamente todas as configurações relevantes ao jogo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INFORMATION,
    "Visualiza as informações sobre a aplicação/núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
    "Valor em ponto flutuante da proporção de tela (largura / altura), utilizado se Proporção de Tela estiver definido como 'Configuração'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Personaliza a altura da janela de exibição que é usada se a Proporção de Tela estiver definida como 'Personalizada'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Personaliza a largura da janela de exibição que é usada se a Proporção de Tela estiver definida como 'Personalizada'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
    "Deslocamento personalizado no eixo-X da janela de exibição. Será ignorado se a 'Escala em Inteiros' estiver habilitada. Neste caso ela será centralizada automaticamente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
    "Deslocamento personalizado no eixo-Y da janela de exibição. Será ignorado se a 'Escala em Inteiros' estiver habilitada. Neste caso ela será centralizada automaticamente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
    "Use Servidor de Retransmissão"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
    "Encaminha conexões de jogo em rede através de um servidor 'homem no meio' (MITM). Útil se o anfitrião estiver atrás de um firewall ou tiver problemas de NAT/UPnP."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
    "Localização do Servidor de Retransmissão"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
    "Escolha um servidor de retransmissão específico para usar. Locais geograficamente mais próximos tendem a ter menor latência."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
    "Adicionar ao mixer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
    "Adicionar ao mixer e reproduzir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
    "Adicionar ao mixer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
    "Adicionar ao mixer e reproduzir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
    "Filtrar por núcleo atual"
    )
MSG_HASH(
    MSG_AUDIO_MIXER_VOLUME,
    "Volume global do mixer de áudio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
    "Volume global do mixer de áudio (em dB). 0dB é o volume normal, e nenhum ganho será aplicado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
    "Nível de Volume do Mixer de Áudio (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
    "Silenciar Mixer de Áudio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
    "Silencia o mixer de áudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
    "Exibir Atualizador On-line"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
    "Exibi ou oculta a opção 'Atualizador On-line'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Exibir Atualizador de Miniaturas Herdadas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Exibi ou oculta a capacidade de baixar pacotes de miniaturas herdadas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
    "Visualizações"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
    "Exibe elementos na tela de menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
    "Exibir Atualizador de Núcleos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
    "Exibi ou oculta a opção de atualizar núcleos (e arquivos de informação de núcleo)."
    )
MSG_HASH(
    MSG_PREPARING_FOR_CONTENT_SCAN,
    "Preparando a busca de conteúdo..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_DELETE,
    "Excluir núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_DELETE,
    "Remove este núcleo do disco."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
    "Opacidade do Framebuffer"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
    "Modificar a opacidade do framebuffer."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
    "Favoritos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
    "Conteúdo adicionado aos 'Favoritos' vai aparecer aqui."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
    "Músicas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_MUSIC,
    "Músicas que foram reproduzidas aparecem aqui."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
    "Imagens"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_IMAGES,
    "Imagens que foram exibidas aparecem aqui."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
    "Vídeos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_VIDEO,
    "Vídeos que foram reproduzidos aparecem aqui."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
    "Ícones do Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
    "Exibe ícones à esquerda das entradas do menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
    "Otimizar o Esquema no Modo Paisagem"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
    "Ajusta automaticamente o esquema do menu para se adequar a tela com orientação de paisagem."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
    "Girar Automaticamente Barra de Navegação"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
    "Move automaticamente a barra de navegação no lado direito da tela na orientação de paisagem."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Aba de Configurações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Definir Senha para habilitar Aba de Configurações"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
    "Digite a Senha"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
    "Senha correta."
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
    "Senha incorreta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Habilita a aba de configurações. É necessário reiniciar para que a aba apareça."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
    "O fornecimento de uma senha ao ocultar a aba de configurações permite restaurar mais tarde a partir do menu, indo para a aba Menu Principal, selecionando Habilitar aba configurações e inserindo a senha."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
    "Permite que o usuário renomeie os itens nas coleções."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
    "Permitir renomear itens"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RENAME_ENTRY,
    "Renomear o título do item."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
    "Renomear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
    "Exibir 'Carregar Núcleo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
    "Exibi ou oculta a opção 'Carregar Núcleo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
    "Exibir 'Carregar Conteúdo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
    "Exibi ou oculta a opção 'Carregar Conteúdo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
    "Exibir 'Carregar Disco'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
    "Exibi ou oculta a opção 'Carregar Disco'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
    "Exibir 'Dumpar Disco'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
    "Exibi ou oculta a opção 'Dumpar Disco'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
    "Exibir 'Informação'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
    "Exibi ou oculta a opção 'Informação'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
    "Exibir 'Arquivo de Configuração'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
    "Exibi ou oculta a opção 'Arquivo de Configuração'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
    "Exibir 'Ajuda'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
    "Exibi ou oculta a opção 'Ajuda'."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Exibir 'Sair do RetroArch'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Exibi ou oculta a opção 'Reiniciar o RetroArch'."
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Exibir 'Sair do RetroArch'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Exibi ou oculta a opção 'Sair do RetroArch'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
    "Exibir 'Reiniciar o RetroArch'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
    "Exibi ou oculta a opção 'Reiniciar o RetroArch'"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
    "Exibir 'Reiniciar'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
    "Exibi ou oculta a opção 'Reiniciar'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
    "Exibir 'Desligar'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
    "Exibi ou oculta a opção 'Desligar'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
    "Menu Rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
    "Exibi ou oculta elementos na tela de Menu Rápido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Exibir 'Captura de Tela'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Exibi ou oculta a opção 'Captura de Tela'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Exibir 'Salvar/Carregar Estado'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Exibi ou oculta as opções para salvar/carregar estados."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Exibir 'Desfazer Salvar/Carregar Estado'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Exibi ou oculta as opções para abolir o salvar/carregar estado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Exibir 'Adicionar aos Favoritos'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Exibi ou oculta a opção 'Adicionar aos Favoritos'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
    "Exibir 'Iniciar Gravação'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
    "Exibi ou oculta a opção 'Iniciar Gravação'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
    "Exibir 'Iniciar Transmissão'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
    "Exibi ou oculta a opção 'Iniciar Transmissão'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Exibir 'Definir Associação do Núcleo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Exibi ou oculta a opção 'Definir Associação do Núcleo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Exibir 'Redefinir Associação do Núcleo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Exibi ou oculta a opção 'Redefinir Associação do Núcleo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
    "Exibir 'Opções'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
    "Exibi ou oculta a opção 'Opções'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
    "Exibir 'Controles'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
    "Exibi ou oculta a opção 'Controles'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
    "Exibir 'Trapaças'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
    "Exibi ou oculta a opção 'Trapaças'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
    "Exibir 'Shaders'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
    "Exibi ou oculta a opção 'Shaders'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Exibir 'Salvar Redefinição de Núcleo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Exibi ou oculta a opção 'Salvar Redefinição de Núcleo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Exibir 'Salvar Redefinição de Jogo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Exibi ou oculta a opção 'Salvar Redefinição de Jogo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
    "Exibir 'Informação'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
    "Exibi ou oculta a opção 'Informação'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
    "Exibir 'Baixar Miniaturas'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
    "Exibi ou oculta a opção 'Baixar miniaturas'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
    "Ativar 'Notificação de Fundo'"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
    "Notificação de Fundo em Cor Vermelha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
    "Notificação de Fundo em Cor Verde"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
    "Notificação de Fundo em Cor Azul"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
    "Opacidade da Notificação de Fundo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
    "Desabilitar o Modo Quiosque"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
    "Desabilita o Modo Quiosque. É necessária uma reinicialização para que a mudança tenha total efeito."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
    "Habilitar o Modo Quiosque"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
    "Protege a configuração escondendo todas as configurações relacionadas à configuração."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
    "Definir senha para desabilitar o Modo Quiosque"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
    "Fornece uma senha ao habilitar o Modo Quiosque tornando possível desabilitar mais tarde a partir do menu, indo para o Menu Principal, selecionando Desabilitar o Modo Quiosque e inserindo a senha."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD,
    "Digite a Senha"
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
    "Senha correta."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
    "Senha incorreta."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
    "Notificação em Cor Vermelha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
    "Notificação em Cor Verde"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
    "Notificação em Cor Azul"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
    "Exibir contagem de quadros na tela FPS"
    )
MSG_HASH(
    MSG_CONFIG_OVERRIDE_LOADED,
    "Substituição de configuração carregada."
    )
MSG_HASH(
    MSG_GAME_REMAP_FILE_LOADED,
    "Arquivo de remapeamento do jogo carregado."
    )
MSG_HASH(
    MSG_CORE_REMAP_FILE_LOADED,
    "Arquivo de remapeamento principal carregado."
    )
MSG_HASH(
    MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "O Adiantar Quadro foi desativado porque esse núcleo não suporta estados de jogo."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
    "Falha ao salvar o estado do jogo. O Adiantar Quadro foi desativado."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
    "Falha ao carregar o estado do jogo. O Adiantar Quadro foi desativado."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
    "Falha ao criar uma segunda instância. O Adiantar Quadro agora usará apenas uma instância."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Adicione automaticamente conteúdo à lista de reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Verifica automaticamente o conteúdo carregado para que eles apareçam dentro das listas de reprodução."
    )
MSG_HASH(
    MSG_SCANNING_OF_FILE_FINISHED,
    "Verificação do arquivo terminado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
    "Opacidade da Janela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
    "Qualidade da Reamostragem do Áudio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
    "Abaixe esse valor para favorecer o desempenho/baixa latência em relação à qualidade de áudio, aumente se desejar melhor qualidade de áudio à custa do desempenho/baixa latência."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
    "Ver arquivos de shader para mudanças"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
    "Aplicar automaticamente as alterações feitas nos arquivos de shader no disco."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
    "Exibir Botões em Janela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
    "Exibir estatísticas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
    "Exibe estatísticas técnicas na tela."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Preenchimento da borda"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Exibir borda do menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Espessura do preenchimento da borda"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Aumentar a grossura do padrão xadrez da borda do menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Espessura do preenchimento de fundo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Aumentar a grossura do padrão xadrez de fundo do menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
    "Bloquear Proporção de Exibição do Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
    "Garante que o menu seja sempre exibido com a proporção correta. Se desativado, o menu rápido será esticado para corresponder ao conteúdo atualmente carregado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
    "Redimensionamento Interno"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
    "Redimensiona a interface do menu antes de desenhar na tela. Quando usado com o 'Filtro Linear de Menu' ativado, remove artefatos de escala (pixels ímpares) mantendo uma imagem nítida. Tem um impacto significativo no desempenho que aumenta com o nível de redimensionamento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
    "Proporção da Exibição de Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
    "Seleciona a proporção do menu. As proporções widescreen aumentam a resolução horizontal da interface do menu. (Pode exigir uma reinicialização se 'Bloquear Proporção da Exibição do Menu' estiver desabilitado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
    "Usar Esquema de Largura Total"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
    "Redimensiona e posiciona as entradas do menu para aproveitar melhor o espaço disponível na tela. Desabilite isso para usar o esquema clássico de duas colunas de largura fixa."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
    "Efeitos de Sombra"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
    "Ativa as sombras projetadas para texto de menu, bordas e miniaturas. Tem um impacto modesto no desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
    "Animação de Fundo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
    "Ative o efeito de animação de partículas de fundo. Tem um impacto significativo no desempenho."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
    "DESLIGADA"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
    "Neve (Leve)"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
    "Neve (Forte)"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
    "Chuva"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
    "Vórtice"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
    "Campo de Estrela"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
    "Velocidade de animação em segundo plano"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
    "Ajusta a velocidade dos efeitos de animação de partículas de fundo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
    "Suporte a ASCII Estendido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
    "Ativa a exibição de caracteres ASCII não padrão. Necessário para compatibilidade com certos idiomas ocidentais não ingleses. Tem um impacto moderado no desempenho."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
    "Para monitores CRT apenas. Tenta usar a resolução exata do núcleo/jogo e a taxa de atualização."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
    "Trocar para Resolução CRT"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
    "Alterna entre resoluções nativas e ultrawide."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
    "Super Resolução CRT"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
    "Exibir Configurações de Rebobinamento"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
    "Exibi ou oculta as opções de Rebobinamento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
    "Exibi ou oculta as opções de Latência."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
    "Exibir 'Configurações de Latência'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
    "Exibi ou oculta as opções de Sobreposição."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
    "Exibir 'Configurações de Sobreposição'"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
    "Mostrar configurações de esquema de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
    "Mostrar ou ocultar opções de esquema de vídeo."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
    "Ativar Áudio de Menu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
    "Ativa ou desativa o som do menu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
    "Configurações do Mixer"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
    "Visualiza e/ou modifica as configurações do mixer de áudio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_INFO,
    "Informação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
    "&Arquivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
    "&Carregar Núcleo..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
    "&Descarregar Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
    "Sai&r"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
    "&Editar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
    "&Pesquisar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
    "&Visualizar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
    "Docas Fechadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
    "Parâmetros do Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
    "&Opções..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
    "Lembrar Posições da Doca:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
    "Lembrar Geometria da Janela:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
    "Lembrar a Última Aba do Navegador de Conteúdo:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
    "Tema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
    "<Padrão do Sistema>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
    "Escuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
    "Personalizado..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
    "Opções"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
    "&Ferramentas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
    "&Ajuda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
    "Sobre o RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
    "Documentação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
    "Carregar Núcleo Personalizado..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
    "Carregar Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
    "Carregando Núcleo..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NAME,
    "Nome"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
    "Versão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
    "Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
    "Navegador de Arquivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
    "Topo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
    "Subir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
    "Navegador de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
    "Arte da Capa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
    "Captura de Tela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
    "Tela de Título"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
    "Todas as Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
    "Informação do Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
    "<Me pergunte>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
    "Informação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_WARNING,
    "Advertência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ERROR,
    "Erro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
    "Erro de Rede"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
    "Por favor, reinicie o programa para que as alterações entrem em vigor."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOG,
    "Registro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
    "%1 itens"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Solte a imagem aqui"
    )
#ifdef HAVE_QT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
    "Verificação Terminada.<br><br>\n"
    "Para que o conteúdo seja verificado corretamente, você deve em ordem:\n"
    "<ul><li>ter um núcleo compatível já baixado</li>\n"
    "<li>ter os \"Arquivos de Informação de Núcleo\" atualizados via Atualizador On-line</li>\n"
    "<li>ter a \"Base de Dados\" atualizada via Atualizador On-line</li>\n"
    "<li>reiniciar o RetroArch caso alguma das situações acima tenha sido feita</li></ul>\n"
    "E finalmente, o conteúdo deve corresponder as bases de dados existentes <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">aqui</a>. Se ainda não estiver funcionando, considere <a href=\"https://www.github.com/libretro/RetroArch/issues\">enviar um relatório de erro</a>."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
    "Exibir Menu Desktop"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_WIMP,
    "Abre o menu desktop se estiver fechado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
    "Não mostrar isto novamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_STOP,
    "Parar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
    "Associar Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
    "Ocultar Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_HIDE,
    "Ocultar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
    "Cor de Destaque"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
    "&Escolher..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
    "Selecionar Cor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
    "Selecionar Tema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
    "Tema Personalizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
    "O caminho do arquivo está em branco."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
    "O arquivo está vazio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
    "Não foi possível abrir o arquivo para leitura."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
    "Não foi possível abrir o arquivo para ser gravado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
    "O arquivo não existe."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
    "Sugerir o Primeiro Núcleo a Carregar:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ZOOM,
    "Zoom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW,
    "Visualização"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
    "Ícones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
    "Lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
    "Opções de Substituição de Configuração"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
    "Opções para substituir a configuração global."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
    "Irá iniciar a reprodução do fluxo de áudio. Uma vez terminado, removerá o fluxo de áudio atual da memória."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
    "Irá iniciar a reprodução do fluxo de áudio. Uma vez terminado, ele fará um loop e reproduzirá a faixa novamente desde o começo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Irá iniciar a reprodução do fluxo de áudio. Uma vez terminado, ele irá pular para o próximo fluxo de áudio em ordem sequencial e repetirá este comportamento. Útil como um modo de reprodução de álbum."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
    "Isso interromperá a reprodução do fluxo de áudio, mas não o removerá da memória. Você pode começar a reproduzi-lo novamente selecionando 'Reproduzir'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
    "Isso interromperá a reprodução do fluxo de áudio e o removerá completamente da memória."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
    "Ajuste o volume do fluxo de áudio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
    "Adiciona esta faixa de áudio a um compartimento de fluxo de áudio disponível. Se nenhum compartimento estiver disponível no momento, ele será ignorado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
    "Adiciona esta faixa de áudio a um compartimento de fluxo de áudio disponível e reproduz. Se nenhum compartimento estiver disponível no momento, ele será ignorado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
    "Reproduzir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
    "Reproduzir (Loop)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Reproduzir (Sequencial)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
    "Parar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
    "Remover"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
    "Volume"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
    "Núcleo atual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
    "Limpar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
    "Pausar conquistas para a sessão atual (Esta ação ativará Estados de Jogos, Trapaças, Rebobinamento, Pausa e Câmera Lenta)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
    "Continuar conquistas para a sessão atual (Esta ação desabilitará Estados de Jogos, Trapaças, Rebobinamento, Pausa e Câmera Lenta e reiniciará o jogo atual)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
    "No Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
    "No Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
    "No Jogo (Pausado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
    "Jogando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
    "Pausado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
    "Habilitar o Discord"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
    "Habilitar ou desabilitar o suporte ao Discord.\n"
    "OBS: Não funcionará com a versão do navegador, apenas o cliente nativo de desktop."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
    "Entrada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_INPUT,
    "Selecionar o dispositivo de entrada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
    "Saída"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
    "Selecionar o dispositivo de saída."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
    "Volume"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_VOLUME,
    "Definir o volume de saída (%)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
    "Gerenciamento de Energia"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
    "Altera as configurações de gerenciamento de energia."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
    "Modo de Desempenho Sustentado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
    "Suporte de mpv"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
    "Índice"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
    "Ver Coincidência #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
    "Coincidir Endereço: %08X Máscara: %02X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
    "Criar Código de Coincidência #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
    "Excluir Coincidência #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
    "Examinar Endereço: %08X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
    "Descrição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
    "Habilitar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
    "Código"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
    "Manipulador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
    "Tamanho da Memória de Pesquisa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
    "Tipo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
    "Valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
    "Endereço da Memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
    "Máscara do Endereço da Memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
    "Vibrar Quando Memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
    "Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
    "Porta de Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
    "Força Primária da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
    "Duração (ms) da Vibração Primária"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
    "Força Secundária da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
    "Duração (ms) da Vibração Secundária"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
    "Número de Iterações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
    "Aumento de Valor em Cada Iteração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Aumento de Endereço em Cada Iteração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
    "Adicionar Nova Trapaça Depois Desta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
    "Adicionar Nova Trapaça Antes Desta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
    "Copiar Esta Trapaça Depois"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
    "Copiar Esta Trapaça Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
    "Excluir Esta Trapaça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
    "Emulador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_RETRO,
    "RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
    "<Desabilitado>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
    "Ajustar Valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
    "Aumentar por Valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
    "Diminuir por Valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
    "Executar próxima trapaça se valor for igual à memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
    "Executar próxima trapaça se valor for diferente da memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
    "Executar próxima trapaça se valor for menor que a memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
    "Executar próxima trapaça se valor for maior que a memória"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
    "<Desabilitado>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
    "Alterações"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
    "Não Altera"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
    "Aumenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
    "Diminui"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
    "Igual Ao Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
    "Diferente Ao Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
    "Menor Ao Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
    "Maior Ao Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
    "Aumenta o Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
    "Diminui o Valor da Vibração"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
    "1-bit, valor máx. = 0x01"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
    "2-bit, valor máx. = 0x03"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
    "4-bit, valor máx. = 0x0F"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
    "8-bit, valor máx. = 0xFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
    "16-bit, valor máx. = 0xFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
    "32-bit, valor máx. = 0xFFFFFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_0,
    "1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_1,
    "2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_2,
    "3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_3,
    "4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_4,
    "5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_5,
    "6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_6,
    "7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_7,
    "8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_8,
    "9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_9,
    "10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_10,
    "11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_11,
    "12"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_12,
    "13"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_13,
    "14"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_14,
    "15"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_15,
    "16"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_16,
    "Todas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
    "Iniciar ou Continuar a Pesquisa de Trapaças"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
    "Iniciar ou Reiniciar a Pesquisa de Trapaças"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
    "Pesquisar Memória por Valores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
    "Adicionar as %u Coincidências para Sua Lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
    "Ver Lista de %u Coincidências"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
    "Criar Código Desta CoincidênciaCoincidência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
    "Excluir Esta Coincidência"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
    "Adicionar Novo Código no Início"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
    "Adicionar Novo Código no Final"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
    "Excluir Todas as Trapaças"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
    "Recarregar Trapaças Específicas do Jogo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
    "Igual a %u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
    "Menos do que Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
    "Maior que Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
    "Menos ou Igual a Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
    "Maior ou Igual a Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
    "Igual a Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
    "Diferente a Antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
    "Igual a Depois+%u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
    "Igual a Antes-%u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
    "Iniciar ou Continuar Pesquisa de Trapaças"
    )
MSG_HASH(
    MSG_CHEAT_INIT_SUCCESS,
    "Pesquisa de trapaças iniciada corretamente"
    )
MSG_HASH(
    MSG_CHEAT_INIT_FAIL,
    "Falha ao iniciar a pesquisa de trapaças"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_NOT_INITIALIZED,
    "A pesquisa não foi iniciada"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_FOUND_MATCHES,
    "Número de coincidências  = %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
    "Big Endian"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
    "Adicionadas %u coincidências"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
    "Falha ao adicionar coincidências"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
    "Código criado da coincidência"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
    "Falha ao criar o código"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
    "Excluir coincidência"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
    "Não há espaço suficiente. O máximo é 100 trapaças"
    )
MSG_HASH(
    MSG_CHEAT_ADD_TOP_SUCCESS,
    "Nova trapaça adicionada ao início da lista."
    )
MSG_HASH(
    MSG_CHEAT_ADD_BOTTOM_SUCCESS,
    "Nova trapaça adicionada ao final da lista."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
    "Pressione direita cinco vezes para excluir todas as trapaças."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_ALL_SUCCESS,
    "Todas as trapaças foram excluídas."
    )
MSG_HASH(
    MSG_CHEAT_ADD_BEFORE_SUCCESS,
    "Nova trapaça adicionada antes deste."
    )
MSG_HASH(
    MSG_CHEAT_ADD_AFTER_SUCCESS,
    "Nova trapaça adicionada depois deste."
    )
MSG_HASH(
    MSG_CHEAT_COPY_BEFORE_SUCCESS,
    "Trapaça copiada antes deste."
    )
MSG_HASH(
    MSG_CHEAT_COPY_AFTER_SUCCESS,
    "Trapaça copiada depois deste."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_SUCCESS,
    "Trapaça excluída."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
    "Progresso:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
    "\"Todas as Listas de Repr.\" Máx. Entradas em Lista:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
    "\"Todas as Listas de Repr.\" Máx. Entradas em Grade:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
    "Mostrar arquivos e pastas ocultas:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
    "Nova Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
    "Por favor, insira o novo nome da lista de reprodução:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
    "Excluir Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
    "Renomear Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
    "Tem certeza de que deseja excluir a lista de reprodução \"%1\"?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_QUESTION,
    "Pergunta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
    "Não foi possível excluir o arquivo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
    "Não foi possível renomear o arquivo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
    "Coletando lista de arquivos..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
    "Adicionando arquivos à lista de reprodução..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
    "Entrada da Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
    "Nome:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
    "Caminho:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
    "Núcleo:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
    "Banco de Dados:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
    "Extensões:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
    "(separado por espaço; inclui tudo por padrão)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
    "Filtrar dentro dos arquivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
    "(usado para encontrar miniaturas)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
    "Tem certeza de que deseja excluir o item \"%1\"?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
    "Por favor, primeiro escolha uma única lista de reprodução."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE,
    "Excluir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
    "Adicionar Entrada..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
    "Adicionar Arquivo(s)..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
    "Adicionar Pasta..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_EDIT,
    "Editar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
    "Selecionar Arquivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
    "Selecionar Pasta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
    "<múltiplos>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
    "Erro ao atualizar a entrada da lista de reprodução."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
    "Por favor, preencha todos os campos requeridos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
    "Atualizar o RetroArch (nightly)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
    "O RetroArch foi atualizado com sucesso. Por favor, reinicie o aplicativo para que as alterações entrem em vigor."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
    "Falha na atualização."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
    "Colaboradores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
    "Shader atual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
    "Move para Baixo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
    "Move para Cima"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD,
    "Carregar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SAVE,
    "Salvar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_REMOVE,
    "Remover"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
    "Remover Passadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_APPLY,
    "Aplicar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
    "Adicionar Passada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
    "Limpar Todas as Passadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
    "Não há passada de shader."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
    "Restaurar Passada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
    "Restaurar Todas as Passadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
    "Restaurar Parâmetro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
    "Baixar miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
    "Um download já está em progresso."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
    "Iniciar na lista de reprodução:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Tipo de visualização de miniatura de ícones:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Limite de cache de miniaturas:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
    "Baixar Todas as Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
    "Sistema Inteiro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
    "Esta Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
    "Miniaturas baixadas com sucesso."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
    "Sucedido: %1 Falhou: %2"
    )
MSG_HASH(
    MSG_DEVICE_CONFIGURED_IN_PORT,
    "Configurado na porta:"
    )
MSG_HASH(
    MSG_FAILED_TO_SET_DISK,
    "Falha ao definir o disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
    "Opções de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Vsync Adaptativo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "O V-Sync é ativado até o desempenho ficar abaixo da taxa de atualização desejada.\n"
    "Pode minimizar as travadas quando o desempenho cai abaixo do tempo real e pode ser mais eficiente em termos energéticos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "Trocar para Resolução CRT"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "Saída nativa, sinais de baixa resolução para uso com monitores CRT."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Alterne entre essas opções se a imagem não estiver centralizada corretamente no visor."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "Centralização do Eixo-X"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Use uma taxa de atualização personalizada especificada no arquivo de configuração, se necessário."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Usar Taxa de Atualização Personalizada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "Selecione a porta de saída conectada ao monitor CRT."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "ID da Saída de Exibição"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Iniciar Gravação"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Inicia a gravação."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Parar Gravação"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Para a gravação."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Iniciar Transmissão"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Inicia a transmissão."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Parar Transmissão"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Para a transmissão."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
    "Alternar gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
    "Alternar transmissão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
    "Serviço de IA"
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
    "Um estado do jogo foi carregado, Conquistas no Modo Hardcore foram desativadas para a sessão atual. Reinicie para ativar o modo hardcore."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Qualidade da Gravação"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Qualidade da Transmissão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "URL da Transmissão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "Porta da Transmissão UDP"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
    "Chave da Transmissão do Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
    "Chave da Transmissão do YouTube"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
    "Modo de Transmissão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
    "Título da Transmissão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Restaurar aos Padrões"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Restaura a configuração atual para os valores padrão."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
    "Tema de Cor do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
    "Branco Básico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
    "Preto Básico"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Selecione um tema de cor diferente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
    "Recolher a barra lateral"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
    "Ter a barra lateral esquerda sempre recolhida."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
    "Truncar nomes da Lista de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
    "Quando ativado, remove os nomes do sistema das listas de reprodução. Por exemplo, exibe 'PlayStation' em vez de 'Sony - PlayStation'. As alterações exigem uma reinicialização para entrar em vigor."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
    "Reduzir Textos Longos dos Metadados"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
    "Ao habilitar esta opção, cada elemento dos metadados de um conteúdo a ser exibido na barra direita das listas de reprodução (núcleo associado, tempo de jogo...) vai ocupar apenas uma linha, cadeias que excedem a largura da barra se moverão automaticamente. Ao desabilitar esta opção, cada elemento dos metadados é apresentado estaticamente, estendendo as linhas conforme necessário."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
    "Use a cor preferida do tema do sistema"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
    "Usar a cor do tema do seu sistema operacional (se houver) - substitui as configurações do tema."
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_LOWEST,
    "Muito baixa"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_LOWER,
    "Baixa"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_NORMAL,
    "Normal"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_HIGHER,
    "Alta"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_HIGHEST,
    "Muito alta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "Nenhuma música disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "Nenhum vídeo disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "Nenhuma imagem disponível."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "Não há favoritos disponíveis."
    )
MSG_HASH(
    MSG_MISSING_ASSETS,
    "Aviso: Recursos ausentes, use o Atualizador On-line se disponível"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
    "Lembrar Tamanho e Posição da Janela"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HOLD_START,
    "Segurar Start (2 segundos)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
    "Salvar listas de reprodução usando o formato antigo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Mostrar núcleos associados nas listas de reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Especifique quando marcar entradas da lista de reprodução com o núcleo atualmente associado (se houver). NOTE: essa configuração é ignorada quando as sub-etiquetas da lista de reprodução estão ativadas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
    "Histórico e Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
    "Sempre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
    "Nunca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
    "Histórico e Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
    "Todas as Listas de Reprodução"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
    "DESLIGADO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
    "Ordenar listas de reprodução por ordem alfabética"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
    "Classifica as listas de reprodução de conteúdo em ordem alfabética. Observe que listas de reprodução de 'histórico' de jogos, imagens, músicas e vídeos usados ​​recentemente são excluídos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
    "Menu de Sons"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
    "Ativar som de OK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
    "Ativar som de cancelamento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
    "Ativar som de aviso"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
    "Ativar som BGM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
    "Baixo + Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
    "Seu driver de gráficos não é compatível com o driver de vídeo atual no RetroArch, voltando para o driver %s. Por favor, reinicie o RetroArch para que as mudanças entrem em vigor."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
    "Suporte a CoreAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
    "Suporte a CoreAudio V3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
    "Salvar Registro de Tempo de Execução do Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
    "Mantém o controle de quanto tempo seu conteúdo está sendo executado ao longo do tempo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Salvar Registro de Tempo de Execução (agregar)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Mantém o controle de quanto tempo cada item do conteúdo foi executado, registrado como o total agregado em todos os núcleos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
    "Registros de Tempo de Execução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
    "Salve os arquivos de registro de tempo de execução neste diretório."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
    "Mostrar sub-etiquetas da lista de reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
    "Mostra informações adicionais para cada entrada da lista de reprodução, como associação principal atual e tempo de jogo (se disponível). Tem um impacto de desempenho variável."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
    "Núcleo:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
    "Tempo de Jogo:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
    "Última Partida:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Tempo de execução de sub-etiquetas da lista de reprodução"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Seleciona o tipo de registro de tempo de execução a ser exibido nas sub-etiquetas da lista de reprodução. (Observe que o registro de tempo de execução correspondente deve ser ativado por meio do menu de opções 'Salvando')"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
    "Por Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
    "Agregar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
    "Formato da 'Última Partida' em sub-etiquetas de listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
    "Selecione o formato de data e hora da último partida, mostrando o registro de execução. Nota: As opções AM/PM afetam ligeiramente o desempenho em algumas plataformas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HMS,
    "YYYY/MM/DD - HH:MM:SS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HM,
    "YYYY/MM/DD - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MDYYYY,
    "MM/DD/YYYY - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_DM_HM,
    "DD/MM - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MD_HM,
    "MM/DD - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HMS_AM_PM,
    "YYYY/MM/DD - HH:MM:SS (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HM_AM_PM,
    "YYYY/MM/DD - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MDYYYY_AM_PM,
    "MM/DD/YYYY - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_DM_HM_AM_PM,
    "DD/MM - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MD_HM_AM_PM,
    "MM/DD - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Correspondência de arquivos difusos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Ao pesquisar as listas de reprodução de entradas associadas a arquivos compactados, corresponde apenas ao nome do arquivo morto em vez de [nome do arquivo]+[conteúdo]. Habilite isso para evitar entradas de histórico de conteúdo duplicadas ao carregar arquivos compactados."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
    "Enviar Informação de Depuração"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_DEBUG_INFO,
    "Falha ao salvar informações de depuração."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_DEBUG_INFO,
    "Falha ao enviar informações de depuração para o servidor."
    )
MSG_HASH(
    MSG_SENDING_DEBUG_INFO,
    "Enviando informações de depuração..."
    )
MSG_HASH(
    MSG_SENT_DEBUG_INFO,
    "Enviado informações de depuração para o servidor com sucesso. Seu ID é o número %u."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
    "Envia informações de diagnóstico sobre o seu dispositivo e a configuração do RetroArch aos nossos servidores para análise."
    )
MSG_HASH(
    MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
    "Pressione mais duas vezes para enviar informações de diagnóstico para a equipe do RetroArch."
    )
MSG_HASH(
    MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
    "Pressione mais uma vez para enviar informações de diagnóstico para a equipe do RetroArch."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
    "Vibrar ao Pressionar a Tecla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
    "Ativar Vibração do Dispositivo (para núcleos suportados)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_DIR,
    "Registros de Eventos do Sistema"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_DIR,
    "Salva os arquivos de registro de eventos do sistema nesse diretório."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
    "Widgets do Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
    "Shaders de Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
    "Verificar sem correspondência do núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
    "Quando desativado, o conteúdo só é adicionado às listas de reprodução se você tiver um núcleo instalado que suporte sua extensão. Ao ativar isso, ele será adicionado à lista de reprodução independentemente. Desta forma, você pode instalar o núcleo que você precisa mais tarde, após a digitalização."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
    "Destacar Ícone de Animação Horizontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
    "Animação Move Para Cima/Baixo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
    "Animação do Menu Principal Abre/Fecha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
    "Índice da GPU"
    )
MSG_HASH(
    MSG_DUMPING_DISC,
    "Dumpando disco..."
    )
MSG_HASH(
    MSG_DRIVE_NUMBER,
    "Unidade %d"
    )
MSG_HASH(
    MSG_LOAD_CORE_FIRST,
    "Por favor, carregue um núcleo primeiro."
    )
MSG_HASH(
    MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
    "Falha ao ler da unidade. Processo de dumpagem abortada."
    )
MSG_HASH(
    MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
    "Falha ao escrever para o disco. Processo de dumpagem abortada."
    )
MSG_HASH(
    MSG_NO_DISC_INSERTED,
    "Nenhum disco inserido na unidade."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Informação do Disco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "Ver informações sobre discos de mídia inseridos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET,
    "Reiniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
    "Reiniciar Todos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Nível de Registro do Frontend"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Define o nível de registro para o frontend. Se um nível de registro emitido pelo frontend estiver abaixo desse valor, ele será ignorado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
    "Intervalo de atualização da taxa de quadros (em quadros)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
    "A exibição da taxa de quadros será atualizada no intervalo definido (em quadros)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Exibir 'Reiniciar Conteúdo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Exibi ou oculta a opção 'Reiniciar Conteúdo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Exibir Fechar Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Exibi ou oculta a opção 'Fechar Conteúdo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Exibir Continuar Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Exibi ou oculta a opção 'Continuar Conteúdo'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
    "Configurações"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
    "Exibi ou oculta elementos na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Exibir Entrada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Exibi ou oculta 'Configurações de Entrada' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "Serviço de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "Altera as configurações do Serviço de IA (Tradução/TTS/Diversos)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
    "Saída do Serviço de IA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
    "URL do Serviço de IA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
    "Serviço de IA Habilitado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
    "Interrompe o jogo durante a tradução (modo Imagem) ou continua em execução (modo Fala)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
    "Um endereço http:// url apontando para o serviço de tradução a ser usado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
    "Habilita o Serviço IA para ser executado quando a tecla de atalho do Serviço de IA for pressionada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
    "Idioma Alvo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
    "O idioma para o qual o serviço será traduzido. Se definido como 'Não Importa', o padrão será inglês."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
    "Idioma Fonte"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
    "O idioma do qual o serviço será traduzido. Se definido como 'Não Importa', ele tentará detectar automaticamente o idioma. A configuração para um idioma específico tornará a tradução mais precisa."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "Tcheco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "Dinamarquês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "Sueco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "Croata"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "Catalão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "Búlgaro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "Bengalês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "Basco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "Azerbaijão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "Albanês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "Africâner"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "Estoniano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
    "Filipino"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "Finlandês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "Galego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "Georgiano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "Guzerate"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "Crioulo Haitiano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "Hebraico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HINDI,
    "Hindi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "Húngaro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "Islandês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "Indonésio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "Irlandês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "Canarês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_LATIN,
    "Latin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "Letão"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "Lituano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "Macedônio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "Malaio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "Maltês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
    "Norueguês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
    "Persa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "Romeno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "Sérvio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "Eslovaco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "Esloveno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "Suaíle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "Tâmil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
    "Telugu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "Tailandês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "Ucraniano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_URDU,
    "Urdu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "Galês"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "Iídiche"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
    "Exibir 'Drivers'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
    "Exibe ou oculta as 'Configurações do Driver' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
    "Exibir 'Vídeo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
    "Exibe ou oculta as 'Configurações de Vídeo' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
    "Exibir 'Áudio'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
    "Exibe ou oculta as 'Configurações de Áudio' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
    "Exibir 'Latência'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
    "Exibe ou oculta as 'Configurações de Latência' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
    "Exibir 'Núcleo'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
    "Exibe ou oculta as 'Configurações de Núcleo' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
    "Exibir 'Configuração'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
    "Exibe ou oculta as 'Definições de Configurações' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
    "Exibir 'Salvando'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
    "Exibe ou oculta as 'Configurações de Salvamento' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
    "Exibir 'Registro de Eventos'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
    "Exibe ou oculta as 'Configurações de Registro de Eventos' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
    "Exibir 'Controle de Quadros'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
    "Exibe ou oculta as 'Configurações do Controle de Quadros' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
    "Exibir 'Gravação'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
    "Exibe ou oculta as 'Configurações de Gravação' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
    "Exibir 'Exibição na Tela'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
    "Exibe ou oculta as 'Configurações de Exibição na Tela' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
    "Exibir 'Interface de Usuário'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
    "Exibe ou oculta as 'Configurações de Interface de Usuário' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
    "Exibir Serviço de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
    "Exibe ou oculta as 'Configurações de Serviço de IA' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
    "Exibir 'Gerenciamento de Energia'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
    "Exibe ou oculta as 'Configurações de Gerenciamento de Energia' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
    "Exibir 'Conquistas'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
    "Exibe ou oculta as 'Configurações de Conquistas' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
    "Exibir 'Rede'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
    "Exibe ou oculta as 'Configurações de Rede' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
    "Exibir 'Listas de Reprodução'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
    "Exibe ou oculta as 'Configurações de Listas de Reprodução' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
    "Exibir 'Usuário'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
    "Exibe ou oculta as 'Configurações do Usuário' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
    "Exibir 'Diretório'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
    "Exibe ou oculta as 'Configurações de Diretório' na tela de Configurações."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_DISC,
    "Carregar um disco de mídia físico. Você deve primeiro selecionar o núcleo (Carregar Núcleo) que pretende usar com o disco."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DUMP_DISC,
    "Dumpa o disco de mídia física no armazenamento interno. Será salvo como um arquivo de imagem."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Modo Imagem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Modo Fala"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
    "Remover"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
    "Remove as predefinições de shader de um tipo específico."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
    "Remover Predefinição Global"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
    "Remove a Predefinição Global, usada por todo o conteúdo e todos os núcleos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
    "Remover Predefinição de Núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
    "Remove a Predefinição de Núcleo, usada por todo o conteúdo executado com o núcleo atualmente carregado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
    "Remover Predefinição do Diretório de Conteúdo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
    "Remove a Predefinição do Diretório de Conteúdo, usada por todo o conteúdo dentro do diretório de trabalho atual."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
    "Remover Predefinição de Jogo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
    "Remove a Predefinição de Jogo, usada apenas para o jogo específico em questão."
    )
MSG_HASH(
    MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
    "Predefinição de shader removida com sucesso."
    )
MSG_HASH(
    MSG_ERROR_REMOVING_SHADER_PRESET,
    "Erro ao remover a Predefinição do shader."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
    "Contador de Tempo de Quadro"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
    "Ajusta as configurações que influenciam o contador de tempo do quadro (ativo somente quando o vídeo paralelizado está desabilitado)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
    "Reiniciar Após Avanço Rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
    "Reinicia o contador de tempo do quadro após o avanço rápido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
    "Reiniciar Após Carregar Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
    "Reiniciar o contador de tempo do quadro após carregar um estado de jogo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
    "Reiniciar Após Salvar Estado de Jogo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
    "Reiniciar o contador de tempo do quadro depois de salvar um estado de jogo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
    "Usa animações decoradas modernas, notificações, indicadores e controles em vez do sistema antigo de texto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
    "A animação que é acionada ao rolar entre as abas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
    "A animação que é acionada ao mover para cima ou para baixo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
    "A animação que é acionada ao abrir um submenu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
    "Excluir lista de reprodução"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
    "Ponto de Acesso Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
    "Habilita ou desabilita o Ponto de Acesso Wi-Fi."
    )
MSG_HASH(
    MSG_LOCALAP_SWITCHING_OFF,
    "Desconectando do Ponto de Acesso Wi-Fi."
    )
MSG_HASH(
    MSG_WIFI_DISCONNECT_FROM,
    "Desconectando do Wi-Fi '%s'"
    )
MSG_HASH(
    MSG_LOCALAP_ALREADY_RUNNING,
    "Ponto de Acesso Wi-Fi já iniciado"
    )
MSG_HASH(
    MSG_LOCALAP_NOT_RUNNING,
    "O Ponto de Acesso Wi-Fi não está em execução"
    )
MSG_HASH(
    MSG_LOCALAP_STARTING,
    "Iniciando Ponto de Acesso Wi-Fi com o SSID=%s e a Senha=%s"
    )
MSG_HASH(
    MSG_LOCALAP_ERROR_CONFIG_CREATE,
    "Não foi possível criar um arquivo de configuração para o Ponto de Acesso Wi-Fi."
    )
MSG_HASH(
    MSG_LOCALAP_ERROR_CONFIG_PARSE,
    "Arquivo de configuração incorreto: valores APNAME ou PASSWORD não puderam ser encontrados em %s"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
    "Permitir que os núcleos alterem o driver de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
    "Permite que os núcleos forcem um driver de vídeo diferente do que está em uso."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
    "Alternar Pausa do Serviço de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
    "Pausa o núcleo enquanto enquanto a tela está sendo traduzida."
    )
