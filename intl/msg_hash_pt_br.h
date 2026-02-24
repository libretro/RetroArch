#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/*
##### NOTE FOR TRANSLATORS ####

PLEASE do NOT modify any `msg_hash_*.h` files, besides `msg_hash_us.h`!

Translations are handled using the localization platform Crowdin:
https://crowdin.com/project/retroarch

Translations from Crowdin are applied automatically and will overwrite
any changes made to the other localization files.
As a result, any submissions directly altering `msg_hash_*.h` files
other than `msg_hash_us.h` will be rejected.
*/

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   "Menu principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Configurações"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favoritos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Histórico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Imagens"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Músicas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Vídeos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Explorar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Núcleos sem conteúdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importar conteúdo"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Menu rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Acesso rápido as configurações relevantes do jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Carregar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Carrega um núcleo para ser executado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Fechar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Fecha o núcleo carregado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Procura executar um núcleo libretro. O navegador começa no seu diretório \"Núcleos\". Caso esteja em branco, começará na raiz (root).\nCaso o diretório \"Núcleos\" seja um diretório, o menu usará o diretório como uma pasta. Caso o diretório \"Núcleos\" seja um endereço completo, começará na pasta do arquivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Carregar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Carrega um conteúdo para ser executado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Procura por conteúdo. Para carregar um conteúdo, é necessário um \"Núcleo\" e um arquivo de conteúdo.\nPara escolher onde o menu começará a procurar por contéudo, defina o diretório \"Navegador de arquivos\". Caso não seja defino, começará na raiz (root).\nO navegador filtrará as extensões com base no último núcleo definido no \"carregar núcleo\" e usará o núcleo quando um conteúdo for carregado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Carregar disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Carrega um disco de mídia físico.\nOBSERVAÇÃO: carregue um núcleo primeiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Descarregar disco"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Descarrega a mídia física para o armazenamento interno como um arquivo de imagem."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Ejetar disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Ejeta o disco da unidade física de CD/DVD."
   )
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Playlists dos conteúdos analisados compatíveis com o banco de dados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Cria e atualiza as playlists ao analisar conteúdos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Abrir menu tradicional"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Abre o menu tradicional da área de trabalho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Desativar modo quiosque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Desativa o modo quiosque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Atualizador Online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Baixa complementos, componentes e conteúdo para o RetroArch."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Entra ou hospeda uma sessão da Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Configurações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Configura o programa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Exibe as informações do sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Arquivo de Configuração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Gerencia e cria arquivos de configuração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Ajuda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Saiba mais sobre como o programa funciona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Reinicia o RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Sair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Fecha o RetroArch.\n\"Salvar configuração ao sair\" está ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_NOW,
   "Sincronizar Agora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW,
   "Acionar manualmente a sincronização com a nuvem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Resolver Conflitos: Manter Local"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Resolver todos os conflitos enviando arquivos locais para o servidor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Resolver Conflitos: Manter Servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Resolver todos os conflitos baixando arquivos do servidor, substituindo as cópias locais."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Fecha o RetroArch.\n\"Salvar configuração ao sair\" está desativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Sair do RetroArch. Matar o programa de qualquer maneira difícil (SIGKILL, etc.) encerrará o RetroArch sem salvar a configuração. Em sistemas baseados em Unix, SIGINT/SIGTERM permite uma 'desinicialização' limpa que inclui salvamento de configuração se ativado."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Baixar um núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Baixa e instala um núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Instalar ou restaurar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Instala ou restaura um núcleo do diretório \"Downloads\"."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Iniciar processador de vídeo"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Iniciar Remote RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Diretório inicial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Explorar arquivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Carregar Arquivo"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "O conteúdo adicionado aos \"Favoritos\" aparecerá aqui."
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
   "Vídeos reproduzidos anteriormente aparecerão aqui."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Explorar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Navegue por todo o conteúdo correspondente ao banco de dados por uma interface com pesquisa categorizada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Núcleos sem conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Núcleos instalados que podem funcionar sem ter que carregar conteúdo."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Baixar núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Atualizar núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Atualiza todos os núcleos instalados para a versão mais recente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Alternar os núcleos para as versões da Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Substitua todos os núcleos antigos e que foram manualmente instalados pelas versões mais recentes da Play Store, quando estiver disponível."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Atualizador de miniaturas da lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Baixe miniaturas para entradas da lista de reprodução selecionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Baixar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Baixe conteúdos gratuitos para o núcleo selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Baixar arquivos de sistema do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Baixa os arquivos de sistema auxiliares necessários para operação do núcleo correto e otimizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Atualizar arquivos de informação de núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Atualizar recursos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Atualizar perfis de controle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Atualizar trapaças"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Atualizar bancos de dados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Atualizar sobreposições"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Atualizar sombreamento GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Atualizar sombreamento Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Atualizar sombreamento Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Mostra as informações referentes ao aplicativo/núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Exibe as informações sobre os discos inseridos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Rede"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Exibe as interfaces de rede e endereços IP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Mostra as informações específicas do dispositivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Gerenciador de banco de dados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Exibe os bancos de dados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Gerenciar cursor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Mostra as pesquisas anteriores."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Etiqueta do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Versão do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Fabricante do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Categoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Autor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Permissões"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licença"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Extensões compatíveis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "API gráficas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Endereço"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Suporte a Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Básico (Salvar/Carregar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Serializado (Salvar/Carregar, Retrocesso)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Determinístico (Salvar/Carregar, Retrocesso, Antecipação de Execução, Netplay)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "Nota: 'Arquivos de sistema estão no Diretório de Conteúdo' está habilitado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "Pesquisando em: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Arquivo faltando (obrigatório):"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Arquivo faltando (opcional):"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Arquivo presente (obrigatório):"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Arquivo presente (opcional):"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Trancar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Impede a modificação do núcleo atualmente instalado. Pode ser usado para evitar atualizações indesejadas quando o conteúdo requer uma versão específica do núcleo (por exemplo, conjuntos de ROMs de Arcade) ou quando o formato de ponto de save do próprio núcleo é alterado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Excluir do menu \"Núcleos sem conteúdo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Impede que este núcleo seja exibido na aba/menu 'Núcleos sem conteúdo'. Aplica-se somente quando o modo de exibição é definido como 'Personalizado'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Excluir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Exclui o núcleo do disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Criar cópia de segurança"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Cria uma cópia de segurança do núcleo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Restaurar cópia de segurança"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Instala uma versão anterior do núcleo a partir de uma lista de cópias de segurança."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Excluir cópia de segurança"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Exclui uma versão da lista de cópias de segurança."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Automático]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Data de compilação"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Versão do RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Versão do Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Compilador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Modelo da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Recursos da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Arquitetura da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Núcleos da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Identificador do pacote"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Identificador da Interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "SO da interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Fonte de energia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Driver de contexto de vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Largura da tela (milímetros)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Altura da tela (milímetros)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "DPI da tela"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Descrição"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Gênero"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Conquistas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Categoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Idioma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Região"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Exclusivo de console"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Exclusivo de plataforma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Nota"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Mídia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Controles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Estilo artístico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Jogabilidade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Narrativa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Ritmo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspectiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Ambientação"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Visão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Veículos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Distribuidora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Desenvolvedor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Origem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Franquia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Classificação TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Classificação da revista Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Análise da revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Classificação da revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edição da revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Mês de lançamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Ano de lançamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Classificação BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Classificação ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Classificação ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Classificação PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Hardware de aprimoramento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Classificação CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Número de série"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Suporte para controle analógico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Suporte para vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Suportado para jogo cooperativo"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Carregar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Carrega uma configuração já existente e substitui os valores atuais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Salvar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Substitui o arquivo de configuração atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Salvar nova configuração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Salva a configuração atual em um arquivo separado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Salvar Configuração Como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Salvar a configuração atual como arquivo de configuração personalizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Salvar Configuração Principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Salvar a configuração atual como configuração principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Redefinir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Restaura a configuração atual aos valores padrão."
   )

/* Main Menu > Help */

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Rolar para cima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Rolar para baixo"
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
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Iniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Alternar menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Sair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Alternar teclado"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Altera os drivers utilizados pelo sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Altera as configurações da saída de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Altera as configurações da entrada/saída de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Altera as configurações do controle, teclado e mouse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latência"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Altera as configurações relacionadas a vídeo, áudio e latência dos comandos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Altera as configurações dos núcleos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Configuração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Altera as definições padrões para os arquivos de configuração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Salvamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Altera as configurações de salvamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Sincronização na Nuvem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Alterar configurações de sincronização na nuvem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Habilitar a sincronização da nuvem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Tentativa de sincronizar configurações, sram e salvamentos para um provedor de armazenamento em nuvem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Sincronia de Nuvem Destrutiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Sincronização: Pontos/Saves"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Sincronização: Arquivos de Configuração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Sincronização: Imagens em Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Sincronização: Arquivos do Sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Quando ativado, pontos/saves serão sincronizados na nuvem."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Quando ativado, os arquivos de configuração serão sincronizados para a nuvem."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Quando ativado, as miniaturas serão sincronizadas a nuvem. Não é recomendado, exceto para coleções grandes de miniaturas customizadas, caso contrário, o baixador de miniaturas é uma escolha melhor."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Quando ativado, os arquivos do sistema serão sincronizados para a nuvem. Isto pode aumentar significativamente o tempo necessário para sincronizar; usar com cuidado."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Quando desativado, os arquivos são movidos para uma pasta de backup antes de serem substituídos ou excluídos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE,
   "Modo de Sincronização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE,
   "Automático: Sincroniza na inicialização do RetroArch e quando os núcleos são descarregados. Manual: Só sincroniza quando o botão 'Sincronizar Agora' for acionado manualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC,
   "Automático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Sincronização da nuvem Backend"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Qual protocolo de rede de armazenamento na nuvem usar?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "Link do Armazenamento na Nuvem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "A URL para a entrada de API aponta para o serviço de armazenamento em nuvem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Nome de usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Seu nome de usuário para sua conta de armazenamento na nuvem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Senha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Sua senha para sua conta de armazenamento na nuvem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Registro de eventos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Altera as configurações de registro de eventos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Navegador de arquivos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Altera as configurações do navegador de arquivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Arquivo de configuração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Arquivo comprimido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Gravando arquivo de configuração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Arquivo do cursor do banco de dados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Arquivo de configuração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Arquivo de predefinição de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Arquivo de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Arquivo de remapeamento de controles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Arquivo de trapaça."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Arquivo de sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Arquivo de banco de dados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Arquivo de fonte TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Arquivo simples."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Vídeo. Selecione para abrir este arquivo com o player de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Música. Selecione para abrir este arquivo com o reprodutor de música."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Arquivo de imagem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Imagem. Selecione para abrir este arquivo com o visualizador de imagem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Núcleo Libretro. Selecionar isto irá associar este núcleo ao jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Núcleo Libretro. Selecione este arquivo para que o RetroArch carregue este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Diretório. Seleccione-o para abrir este diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Controle de quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Altera as configurações de retrocesso, avanço rápido e câmera lenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Gravação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Altera as configurações de gravação."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Exibição na tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Altera as configurações de sobreposição de tela e teclado, e as notificações na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Interface de usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Altera as configurações da interface de usuário."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Serviço de IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Altera as configurações do serviço de IA (Tradução/TTS/Diversos)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Acessibilidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Altera as configurações do narrador de acessibilidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Gerenciamento de energia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Altera as configurações de gerenciamento de energia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Conquistas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Altera as configurações de conquistas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Rede"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Define as configurações do servidor e da rede."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Altera as configurações das playlists."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Altera as configurações de privacidade, conta e nome de usuário."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Diretório"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Altera os diretórios padrões onde os arquivos estão localizados."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Mapeamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Mídia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Desempenho"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Áudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Especificação"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Armazenamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Cronometragem"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Altera as configurações relacionadas ao Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Driver de entrada a ser usado. Alguns drivers de vídeo forçam um driver de entrada diferente. (Reinício necessário)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "O driver udev lê eventos evdev para suporte ao teclado. Ele também suporta callback de teclado, mouses e touchpads.\nPor padrão na maioria das distros, os nós /dev/input são root-only (modo 600). Você pode configurar uma regra udev que os torna acessíveis para non-root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "O driver de entrada linuxraw requer um TTY. Os eventos do teclado são lidos diretamente do TTY, o que o torna mais simples, mas não tão flexível quanto o udev. Mouses, etc não são suportados. Este driver usa a API mais antiga do joystick (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Driver de Entrada. O driver de video pode forçar outro driver de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Controle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Define o driver de controle (requer reinício)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Driver de controle DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "𝘋𝘳𝘪𝘷𝘦𝘳 de baixo nível do dispositivo de interface humana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "O driver Linux bruto, usa API de joystick legada. Use udev no lugar se possível."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Driver Linux para controladores de porta paralela conectada via adaptadores especiais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Driver do controle baseado em bibliotecas SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Driver de controles com interface udev é geralmente recomendado. Usa a API recente evdev joypad para suporte ao joystick. Ele suporta hotplugging e feedback forçado.\nPor padrão na maioria das distros, /dev/input nodes são root-only (modo 600). Você pode configurar uma regra udev que os torna acessíveis para não-root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Driver de controle XInput. Geralmente para controles de Xbox."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Define o driver de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Driver OpenGL 1.x. Versão mínima necessária: OpenGL 1.1. Não suporta shaders. Use drivers OpenGL posteriores, se possível."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Driver OpenGL 2.x. Este driver permite que núcleos GL da libretro sejam utilizados além de núcleos renderizados por software. Versão mínima necessária: OpenGL 2.0 ou OpenGLES 2.0. Suporta o formato de shader GLSL. Use o driver glcore em vez desse, se possível."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Driver OpenGL 3.x. Este driver permite que núcleos GL da libretro sejam utilizados além de núcleos renderizados por software. Versão mínima necessária: OpenGL 3.2 ou OpenGLES 3.0. Suporta o formato de shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Driver Vulkan. Este driver permite que os núcleos Vulkan sejam utilizados em adição a núcleos renderizados por software. Versão mínima necessária: Vulkan 1.0. Suporta shaders HDR e Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Driver de renderização por software SDL 1.2. O desempenho é considerado como subideal. Considere usá-lo apenas como último recurso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Driver de renderização por software SDL 2. A performance para implementações de núcleo libretro com processamento por software depende da implementação SDL da sua plataforma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Driver Metal para plataformas Apple. Suporta o formato de shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Driver Direct3D 8 sem suporte para shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Driver Direct3D 9 com suporte para o antigo formato de shader Cg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Driver Direct3D 9 com suporte para o formato de shader HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Driver Direct3D 10 com suporte para o formato de shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Driver Direct3D 11 com suporte para HDR e formato de shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Driver Direct3D 12 com suporte para HDR e formato de shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Driver DispmanX. Usa a API DispmanX para o Videocore IV GPU no Raspberry Pi 0..3. Não há suporte para sobreposição ou sombra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Driver LibCACA. Produz saída em caracteres ao invés de gráficos. Não recomendado para uso prático."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Um driver de vídeo Exynos de baixo nível que usa o bloco G2D em Samsung Exynos SoC para operações iluminadas. O desempenho para núcleos renderizados de \"software\" deve ser ideal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Driver de vídeo DRM simples. Este é um \"driver\" de vídeo de baixo nível usando libdrm para escalar \"hardware\" usando camadas de GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Um driver de vídeo Sunxi de baixo nível que usa o bloco G2D em Allwinner SoCs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Driver do Wii U. Suporta shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Driver do Switch. Suporta o formato de shader GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Driver OpenVG. Usa a API gráfica OpenVG para aceleração de hardware para vetores 2D."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Driver GDI. Usa uma interface legado do Windows. Não recomendado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Driver de vídeo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Define o driver de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Driver RSound para sistemas de áudio em rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Driver legado Open Soud System."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Driver ALSA padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "Driver ALSA com suporte a threading."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Driver ALSA implementado sem dependências."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Driver de sistema de som RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "Driver OpenAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "Driver OpenSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Driver DirectSound. DirectSound é usado principalmente do Windows 95 para o Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Driver API de Sessão de Áudio do Windows. WASAPI é usado principalmente no Windows 7 ou superior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Driver PulseAudio. Se o sistema usa PulseAudio, certifique-se de usar este driver em vez de, por exemplo, ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Driver do PipeWire. Se o sistema usa PipeWire, lembre-se de usar esse driver em vez de e.g. PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Driver de Kit de Conexão Jack Áudio."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Microfone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Define o driver de microfone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Reamostrador de microfone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Driver de reamostragem de áudio a ser utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Quadros de Bloqueio do Microfone"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Reamostragem de áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Driver de reamostragem de áudio a ser utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Implementação do Sinc em janela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Implementação do cosseno convoluto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Implementação de reamostragem Nearest. Este reamostrador ignora a configuração de qualidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Câmera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Define o driver de câmera."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Define o driver de Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Define o driver de Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Localização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Define o driver de localização."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Define o driver do menu (requer reinício)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB é uma interface gráfica para o RetroArch que se parece com o menu de console da 7ª geração. Suporta os mesmos recursos que a Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone é a GUI padrão de RetroArch na maioria das plataformas. Ela é otimizada para navegação com um controle de jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI é uma GUI simples integrada para RetroArch. Tem os requisitos de desempenho mais baixos entre os drivers de menu e pode ser usado em telas de baixa resolução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Em dispositivos móveis, o RetroArch usa a UI móvel, MaterialUI, por padrão. Essa interface é projetada em torno da tela de toque e dispositivos de ponteiro como um mouse/trackball."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Gravação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Define o driver de gravação."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Define o driver MIDI."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Trocar para resolução CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Saída nativa, sinais de baixa resolução para uso com monitores CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Saída"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Altera as configurações da saída de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Modo tela cheia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Altera as configurações do modo tela cheia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Modo janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Altera as configurações do modo janela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Escala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Altera as configurações de escala de vídeo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Altere as configurações de vídeo do HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Sincronização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Altera as configurações de sincronização de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Desativar protetor de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Evite que o protetor de tela do sistema seja ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Suspende o protetor de tela. É uma dica que não precisa necessariamente ser honrada pelo driver de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Vídeo paralelizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Melhora o desempenho ao custo de uma maior latência e mais engasgadas no sinal de vídeo. Use apenas caso não seja possível obter a velocidade máxima de outra forma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Use um driver de vídeo multitarefa. Usar isto pode melhorar o desempenho a custo de possível latência e mais travamentos de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Inserção de quadro opaco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "AVISO: Tremulação rápida pode causar persistência de imagem em algumas telas. Use por sua conta em risco // Insira quadro(s) preto(s) entre os quadros. Pode reduzir significativamente o desfoque de movimento emulando a varredura de CRT, mas ao custo do brilho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Insere quadro(s) preto(s) entre os quadros para melhorar a clareza do movimento. Use apenas a opção designada para a taxa de atualização atual da sua tela. Não deve ser usado em taxas de atualização que não sejam múltiplas de 60Hz, como 144Hz, 165Hz, etc. Não combine com Intervalo de Troca (Swap Interval) > 1, sub-quadros, Atraso de Quadros (Frame Delay) ou Sincronizar com a Taxa de Quadros Exata do Conteúdo. Deixar o VRR do sistema ligado não tem problema, apenas não utilize essa c[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Inserção de Quadros Pretos - Quadros Escuros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Ajuste o número de quadros pretos na sequência total de escaneamento BFI. Mais equivalea maior nitidez, menos é igual a maior brilho. Não se aplica ao 120hz, pois há apenas 1 quadro BFI para trabalhar com o total. As configurações maiores que o possível vão limitar ao máximo possível para a taxa de atualização escolhida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Ajusta o número de quadros exibidos na sequência BFI que são pretos. Mais quadros pretos aumentam a nitidez, mas reduzem o brilho. Não aplicável ao 120hz, pois há apenas um quadro extra de 60hz no total, por isso, deve ser preto, caso contrário o BFI não estaria de modo algum ativa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Subquadros de sombreamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "AVISO: Tremulação rápida pode causar persistência de imagem em algumas telas. Use por sua conta em risco // Simula uma varredura básica sobre vários subquadros dividindo a tela verticalmente e renderizando cada parte da tela de acordo com quantos subquadros há."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Insere quadro(s) de shader extra(s) entre os quadros para quaisquer efeitos de shader que foram projetados para rodar mais rápido que a taxa do conteúdo. Use apenas a opção designada para a taxa de atualização atual da sua tela. Não deve ser usado em taxas de atualização que não sejam múltiplas de 60Hz, como 144Hz, 165Hz, etc. Não combine com Intervalo de Troca (Swap Interval) > 1, BFI, Atraso de Quadros (Frame Delay) ou Sincronizar com a Taxa de Quadros Exata do Conteúdo. Deixar o [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Simulação do rolamento da linha de varredura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "AVISO: Tremulação rápida pode causar persistência de imagem em algumas telas. Use por sua conta em risco // Simula uma varredura básica sobre vários subquadros dividindo a tela verticalmente e renderizando cada parte da tela de acordo com quantos subquadros há."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Simula o rolamento básico da linha de varredura em múltiplos sub-quadros, dividindo a tela verticalmente e renderizando cada parte conforme a quantidade de sub-quadros existente no canto superior da tela para baixo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Filtragem bilinear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Adiciona um leve desfoque para suavizar arestas dos píxeis. Esta opção tem um pequeno impacto no desempenho. Deve ser desativada ao usar sombreadores."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Interpolação da imagem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Determina um método para a interpolação da imagem durante o dimensionamento do conteúdo através do IPU interno. Os modos \"Bicúbico\" ou \"Bilinear\" são recomendados caso utilize filtros de processamento de vídeo pela CPU. Esta opção não impacta o desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bicúbico"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Interpolação de imagem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Especifica o método de interpolação de imagem quando a 'Escala Inteira' estiver desativada. 'Nearest Neighbor' tem o menor impacto no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Semi-linear"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Atraso de sombreadores automáticos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Atrasa o carregamento automático dos sombreadores (em ms). Pode solucionar alguns problemas gráficos ao usar um programa de \"captura de tela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Filtro de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Aplica um filtro de vídeo processado pela CPU. Afeta muito o desempenho. Alguns filtros de vídeo podem funcionar apenas nos núcleos que usam cores com 32 ou 16 bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Aplica um filtro de vídeo processado pela CPU. Afeta muito o desempenho. Alguns filtros de vídeo podem funcionar apenas nos núcleos que usam cores com 32 ou 16 bits. Bibliotecas de filtros de vídeo, vinculadas dinamicamente, podem ser selecionadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Aplica filtro de vídeo via CPU. Pode ter um alto custo de desempenho. Alguns filtros funcionam apenas com núcleos que usam cores de 32 ou 16 bits. É possível selecionar bibliotecas de filtros de vídeo integradas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Remover o filtro do vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Descarregue qualquer filtro de vídeo ativo que utilize processamento da CPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Habilitar tela cheia sobre o entalhe (“notch”) em dispositivos Android"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Usar Buffers de Argumento do Metal (Reinício necessário)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Tentar melhorar o desempenho usando buffers de argumento do Metal. Alguns núcleos podem exigir isso. Isso pode causar problemas com alguns shaders, especialmente em hardware antigo ou versões antigas do sistema operacional."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Trocar para resolução CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Para monitores CRT apenas. Tenta usar a resolução exata do núcleo/jogo e a taxa de atualização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Super resolução CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Alterna entre resoluções nativas e ultrawide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Centralização Horizontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Alterne entre essas opções se a imagem não estiver centralizada corretamente no visor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Tamanho Horizontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Percorra essas opções para ajustar as configurações horizontais e alterar o tamanho da imagem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Centralização Vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST,
   "Alterne entre estas opções se a imagem não estiver centralizada corretamente na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Usar Menu de Alta Resolução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Alterna para uma linha de modo em alta resolução para os menus quando o conteúdo não for carregado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Taxa de atualização personalizada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Use uma taxa de atualização personalizada especificada no arquivo de configuração, se necessário."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Índice de monitor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Selecione qual a tela será usada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Monitor preferido. 0 (padrão) significa que nenhum monitor tem a preferência, 1 e acima (1 sendo primeiro monitor), sugere ao RetroArch para usar esse monitor em particular."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Otimizar para o controle do Wii U (necessário reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Usa o dobro da escala (2x) do GamePad como a janela de exibição. Se desativada, usa a resolução nativa da TV."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rotação de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Força uma certa rotação da tela. A rotação é adicionada a rotação que o núcleo definir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Orientação da tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Força uma certa orientação da tela do sistema operacional."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Índice da GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Seleciona qual placa de vídeo será usada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Posição horizontal da tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Força uma certa posição horizontal ao vídeo. A posição é aplicada globalmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Posição vertical da tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Força uma certa posição vertical ao vídeo. A posição é aplicada globalmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Taxa de atualização vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "A taxa de atualização vertical da sua tela. Utilizado para calcular uma taxa adequada da entrada de áudio.\nIsso será ignorado caso a função \"Vídeo paralelizado\" estiver ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Taxa de atualização estimada da tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "A taxa de atualização estimada da tela em Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "A taxa de atualização precisa do seu monitor (Hz). Isso é usado para calcular a taxa de entrada de áudio com a fórmula:\naudio_input_rate = taxa de entrada do jogo * taxa de atualização da tela / taxa de atualização do jogo\nSe o núcleo não relatar nenhum valor, os padrões NTSC serão assumidos para compatibilidade.\nEsse valor deve permanecer próximo de 60Hz para evitar grandes alterações no tom. Se o seu monitor não operar em ou próximo de 60Hz, desative o VSync e deixe este v[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Definir taxa de atualização reportada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "A taxa de atualização conforme relatada pelo driver de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Botão automático de taxa de atualização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Alternar a taxa de atualização da tela automaticamente com base no conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Apenas no modo de tela cheia exclusivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Apenas no modo janela em tela cheia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Todos os modos de tela cheia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Limite da Taxa de Atualização Automática PAL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Taxa de atualização máxima para ser considerado PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Taxa de atualização vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Define a taxa de atualização vertical da tela. \"50 Hz\" irá permitir um vídeo suave ao executar o conteúdo PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Forçar desativação de sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Imponha o desativamento ao suporte sRGB FBO. Alguns drivers Intel OpenGL no Windows têm problemas no vídeo com sRGB FBOs. Ativando esta opção pode resolver o problema."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Exibição em Tela Cheia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Exibir em tela cheia. Pode ser alterado em tempo de execução. Pode ser sobrescrito por um parâmetro de linha de comando."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Modo janela em tela cheia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Em tela cheia, prefira usar uma janela em tela cheia para evitar ter que alternar entre os modos de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Largura em tela cheia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Define a largura personalizada para o modo de tela cheia em não-janela. Deixar desativado irá usar a resolução da área de trabalho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Altura em tela cheia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Define a altura personalizada para o modo de tela cheia em não-janela. Deixar desativado irá usar a resolução da área de trabalho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Forçar resolução em UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Força a resolução ao tamanho da tela. Se o valor definido for 0, será usado o valor fixo de 3840x2160."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Escala no modo janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Defina o tamanho da janela para o múltiplo especificado do tamanho da área de exibição do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Opacidade da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Define a transparência da janela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Mostrar botões em janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Mostra a barra de título e bordas da janela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Mostrar barra de menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Mostra a barra de menu da janela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Lembrar tamanho e posição da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Mostre todo o conteúdo em uma janela com tamanho fixo e dimensões especificadas pela \"Largura da janela\" e pela \"Altura da janela\" e salve o tamanho e a posição atual da janela ao fechar o RetroArch. Quando estiver desativado, o tamanho da janela será definido dinamicamente com base na \"Escala no modo janela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Usar tamanho personalizado da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Mostre todo o conteúdo em uma janela com um tamanho fixo e dimensões especificadas pela \"Largura da janela\" e pela \"Altura da janela\". Quando estiver desativado, o tamanho da janela será definido dinamicamente com base na \"Escala no modo janela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Largura da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Define a largura personalizada para a janela de exibição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Altura da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Definir a altura personalizada para a janela de exibição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Largura máxima da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Ajuste a largura máxima da janela de exibição ao fazer o redimensionamento automático de acordo com a \"Escala no modo janela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Altura máxima da janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Ajuste a altura máxima da janela de exibição ao fazer o redimensionamento automático de acordo com a \"Escala no modo janela\"."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Dimensionar com valores inteiros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Escala o vídeo apenas em passos inteiros. O tamanho base depende da geometria e proporção de aspecto informadas pelo núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Eixo de Escala Inteira"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Escala a altura, a largura ou ambas. Passos intermediários se aplicam apenas a fontes em alta resolução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Escala de Dimensionamento Inteiro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Arredonda para o número inteiro mais próximo (para cima ou para baixo). A opção 'Smart' reduz a escala quando a imagem sofre um corte excessivo e, por fim, recorre à escala não inteira se as margens da escala reduzida forem muito grandes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Reduzir escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Aumentar escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Inteligente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Define a proporção de tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Configurar proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Valor de vírgula flutuante para a proporção de tela (largura / altura)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Configuração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_SQUARE_PIXEL,
   "PAR 1:1 (%u:%u DAR)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Fornecida pelo núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Completa"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Manter a proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Mantem a proporção da tela de pixels 1:1 ao dimensionar o conteúdo usando a IPU interna. Se esta opção for desativada, as imagens serão esticadas para preencher a tela inteira."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Posição X personalizada da proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Deslocamento personalizado da viewport usado para definir a posição no eixo X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Posição Y personalizada da proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Deslocamento personalizado da viewport usado para definir a posição no eixo Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Deslocamento do eixo X do ponto de âncora da viewport"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Deslocamento do eixo X do ponto de âncora da viewport"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Deslocamento do eixo Y do ponto de âncora da viewport"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Deslocamento do eixo Y do ponto de âncora da viewport"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Posição horizontal do conteúdo quando a viewport é mais larga que o conteúdo. 0.0 é à esquerda, 0.5 é o centro, 1.0 é à direita."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Posição vertical do conteúdo quando a viewport é mais alta que o conteúdo. 0.0 é no topo, 0.5 é o centro, 1.0 é na parte inferior."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Deslocamento do Âncora da Viewport no Eixo X (Orientação Retrato)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Deslocamento do Âncora da Viewport no Eixo X (Orientação Retrato)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Deslocamento do Âncora da Viewport no Eixo Y (Orientação Retrato)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Deslocamento do Âncora da Viewport no Eixo Y (Orientação Retrato)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Posição horizontal do conteúdo quando a viewport é mais larga que o conteúdo. 0.0 é totalmente à esquerda, 0.5 é centralizado, 1.0 é totalmente à direita. (Orientação Retrato)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Posição vertical do conteúdo quando a viewport é mais alta que o conteúdo. 0.0 é no topo, 0.5 é centralizado, 1.0 é na parte inferior. (Orientação Retrato)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Largura personalizada da proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Personaliza a largura da janela de exibição que é usada se a proporção de tela estiver definida como \"Proporção de tela personalizada\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Altura personalizada da proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Personaliza a altura da janela de exibição que é usada se a proporção de tela estiver definida como \"Proporção de tela personalizada\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Cortar overscan (requer reinício)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Elimina alguns pixels ao redor das bordas da imagem habitualmente deixada em branco pelos desenvolvedores, que por vezes têm pixels descartados."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Definir o modo de saída HDR se o monitor suportar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_OFF,
   "Desligado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Pico da luminância"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Configure a luminância máxima (em cd/m²) que sua tela consegue reproduzir. Uma vez configurada, não altere. Use a Luminância de Branco Papel para ajustar o brilho. Consulte o RTings.com para ver a luminância máxima da sua tela. Em algumas telas com scanlines ativadas (abaixo), pode ser necessário forçar o aumento disso (e do branco papel) para elevar o brilho geral."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Luminância papel branco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Uma vez que a luminância máxima esteja configurada, use isto como sua configuração padrão de brilho. Tecnicamente, isso define a luminância na qual o branco papel deve estar, ou seja, texto legível ou a luminância no topo da faixa SDR (Standard Dynamic Range)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Reforço de Cor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Usa a gama de cores completa da sua tela para criar uma imagem mais brilhante e saturada. Para cores mais fiéis ao design original do jogo, configure isto como PRECISO."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_ACCURATE,
   "Modo precisão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_EXPANDED,
   "Expandido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_WIDE,
   "Amplo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SCANLINES,
   "Linhas de varredura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SCANLINES,
   "Ativa linhas de varredura HDR. As linhas de varredura são a principal razão para usar HDR no RetroArch, já que uma execução com precisão das linhas desliga a maior parte da tela, e o HDR recupera um pouco desse brilho perdido. Caso seja necessário mais controle sobre as linhas de varredura, veja os sombreadores personalizados fornecidos pelo RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Layout de subpíxel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Define o layout de subpíxel, afeta apenas as linhas de varredura. Caso não sabia o layout de subpixel da sua tela, veja a sessão \"Subpixel Layout\" do seu monitor em Rtings.com."
   )


/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Sincronização vertical (Vsync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Sincronize a saída do vídeo da placa gráfica com a taxa de atualização da tela. Recomendado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Intervalo da troca VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Usa um intervalo personalizado para a sincronização vertical (VSync). Reduz a taxa de atualização do monitor pelo fator especificado. \"Auto\" define o fator com base na taxa de quadros do núcleo, fornecendo quadros melhores ao executar em, por exemplo, conteúdo de 30 QPS em 60 Hz ou conteúdo de 60 QPS em 120 Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Automático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Vsync adaptativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync é ativado até que o desempenho caia abaixo da taxa de atualização alvo. Pode minimizar travamentos quando o desempenho fica abaixo do tempo real, além de ser mais eficiente energeticamente. Não é compatível com 'Atraso de Quadro (Frame Delay)'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Atraso de Quadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Reduz a latência, mas com maior risco de travamentos no vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Define quantos milissegundos devem ser aguardados antes de executar o núcleo após a apresentação do vídeo. Reduz a latência, mas com maior risco de travamentos.\nValores acima de 20 são tratados como porcentagem do tempo de quadro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Atraso automático de quadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Ajusta dinamicamente o 'Atraso de Quadro (Frame Delay)' efetivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Tenta manter o 'Atraso de Quadro' desejado e minimizar perdas de quadro. O ponto inicial é 3/4 do tempo de quadro quando o 'Frame Delay' está em 0 (Automático)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Automático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "efetivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Sincronia rígida de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Sincronia rígida entre CPU e GPU. Reduz a latência ao custo de desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Quadros de sincronia rígida de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Define quantos quadros a CPU pode rodar à frente da GPU quando utilizado o recurso \"Sincronia rígida de GPU\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Define quantos quadros a CPU pode adiantar em relação à GPU ao usar 'Sincronização Rígida da GPU (GPU Hard Sync)'. Máximo é 3.\n 0: Sincroniza imediatamente com a GPU.\n 1: Sincroniza com o quadro anterior.\n 2: E assim por diante..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Sincronizar com a Taxa de Quadros Exata do Conteúdo (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Evita desviar do intervalo solicitado pelo núcleo. Use com monitores com uma taxa de atualização variável (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Sincronizar com a Taxa de Quadros Exata do Conteúdo. Essa opção é equivalente a forçar velocidade x1, ainda permitindo avanço rápido. Sem desvios da taxa de atualização solicitada pelo núcleo, sem controle dinâmico da taxa de áudio."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Saída"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Altera as configurações da saída de áudio."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Microfone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Altera as configurações da entrada de áudio."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Sincronização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Altera as configurações de sincronização de áudio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Altera as configurações de MIDI."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Altera as configurações do mixer de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Sons do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Altera as configurações de som do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Desabilitar áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Silencie o áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Silenciar mixer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Silencie o mixer de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Respeitar Modo Silencioso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Silenciar todo o áudio no Modo Silencioso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Áudio Mudo (Avanço Rápido)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Silenciar automaticamente o áudio ao usar o avanço rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Aceleração do Áudio (Avanço Rápido)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Acelerar o áudio ao usar avanço rápido. Previne ruídos, mas aumenta o tom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Áudio Mudo no Retrocesso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Silencia o áudio automaticamente ao usar o retrocesso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Volume (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Volume do áudio (em dB). O volume padrão é 0 dB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Volume do conteúdo. O volume pode ser alterado usando suas teclas de atalho \"Aumentar volume\" e \"Diminuir volume\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Volume do mixer (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Volume do mixer de áudio (em dB). O volume padrão é 0 dB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Plugin DSP de áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Plugin DSP de áudio que processa o áudio antes de ser enviado para o driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Remover o Plugin DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Descarrega qualquer plugin DSP de áudio que esteja ativo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Modo WASAPI exclusivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Permite ao driver WASAPI obter controle exclusivo do dispositivo de áudio. Se desativado, o modo compartilhado será utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Formato WASAPI de ponto flutuante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Utiliza formato de ponto flutuante para o driver WASAPI, se suportado pelo dispositivo de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Tamanho do buffer compartilhado de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "O tamanho (em quadros) do buffer intermediário quando o driver WASAPI estiver em modo compartilhado."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Ative a saída de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Substitui o dispositivo de áudio padrão utilizado pelo driver de áudio. Isto depende do driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Substitui o dispositivo de áudio padrão utilizado pelo driver de áudio. Isto depende do driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Valor personalizado de dispositivo PCM para o driver ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Valor de caminho personalizado para o driver OSS (ex.: /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Valor de nome de porta personalizado para o driver JACK (ex.: system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Endereço IP personalizado de um servidor RSound para o driver RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Latência de áudio (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Latência máxima de áudio em milissegundos. O driver tenta manter a latência real em 50% desse valor. Pode não ser respeitada se o driver de áudio não conseguir fornecer a latência especificada."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Microfone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Ativa entrada de áudio nos núcleos compatíveis. Não causa impacto no desempenho se o núcleo não estiver usando um microfone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Substitui o dispositivo de entrada padrão usado pelo driver de microfone. Isso depende do driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Substitui o dispositivo de entrada padrão usado pelo driver de microfone. Isso depende do driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Qualidade da reamostragem do áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Reduza este valor para favorecer o melhor desempenho/menor latência ao custo da qualidade do áudio, aumente para uma qualidade melhor do áudio à custa da performance/baixa latência."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Taxa de entrada padrão (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Taxa de amostragem de entrada de áudio, usada caso o núcleo não solicite um valor específico."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Latência de Entrada de Áudio (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Latência da entrada de áudio desejada em milissegundos. Pode não ser honrado se o driver do microfone não puder prover a latência desejada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Modo WASAPI exclusivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Permite que o RetroArch assuma controle exclusivo do dispositivo de microfone ao usar o driver de microfone WASAPI. Se desativado, o RetroArch usará o modo compartilhado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Formato WASAPI de ponto flutuante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Utiliza entrada em ponto flutuante para o driver WASAPI, se for compatível com seu dispositivo de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Tamanho do buffer compartilhado de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "O tamanho (em quadros) do buffer intermediário quando o driver WASAPI estiver em modo compartilhado."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Qualidade da reamostragem do áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Reduza este valor para favorecer o melhor desempenho/menor latência ao custo da qualidade do áudio, aumente para uma qualidade melhor do áudio à custa da performance/baixa latência."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Frequência da saída de áudio (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Taxa de amostragem da saída de áudio."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Sincronização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Sincroniza o áudio. Recomendado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Variação máxima da sincronia de áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "A alteração máxima da taxa da entrada de áudio. Se aumentado ativa alterações maiores na regulagem ao custo de imprecisão no timbre do áudio (ex: rodando núcleos PAL em modo NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Desvio máximo de tempo do áudio.\nDefine a alteração máxima na taxa de entrada. Você pode querer aumentar isso para permitir mudanças muito grandes no tempo, por exemplo, ao rodar núcleos PAL em telas NTSC, ao custo de uma afinação de áudio imprecisa.\nA taxa de entrada é definida como:\ninput rate * (1.0 +/- (max timing skew))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Controle dinâmico da frequência de áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Ajuda a suavizar as imperfeições na regulagem ao sincronizar áudio e vídeo. Esteja ciente que se desativado, será quase impossível de se obter a sincronia adequada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Definir este valor como 0 desativa o controle de taxa. Qualquer outro valor controla o delta do controle de taxa de áudio.\nDefine o quanto a taxa de entrada pode ser ajustada dinamicamente. A taxa de entrada é definida como:\ninput rate * (1.0 +/- (rate control delta))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Selecione o dispositivo de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Define o dispositivo de entrada (específico do driver). Quando definido como 'Desligado', a entrada MIDI será desativada. O nome do dispositivo também pode ser digitado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Saída"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Selecione o dispositivo de saída."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Define o dispositivo de saída (específico do driver). Quando definido como 'Desligado', a saída MIDI será desativada. O nome do dispositivo também pode ser digitado.\nQuando a saída MIDI está habilitada e o núcleo e o jogo/aplicativo suportam saída MIDI, alguns ou todos os sons (dependendo do jogo/aplicativo) serão gerados pelo dispositivo MIDI. No caso do driver MIDI 'nulo', isso significa que esses sons não serão audíveis."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Defina o volume de saída (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Fluxi do Mixer #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Reproduzir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Irá iniciar a reprodução do fluxo de áudio. Uma vez terminado, removerá o fluxo de áudio atual da memória."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Reproduzir (repetição)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Irá iniciar a reprodução do fluxo de áudio. Uma vez terminado, ele fará um loop e reproduzirá a faixa novamente desde o começo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Reproduzir (sequencial)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Irá iniciar a reprodução do fluxo de áudio. Uma vez terminado, ele irá pular para o próximo fluxo de áudio em ordem sequencial e repetirá este comportamento. Útil como um modo de reprodução de álbum."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Parar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Isso vai interromper a reprodução do fluxo de áudio, mas não vai removê-lo da memória. Pode ser iniciado novamente selecionando \"Reproduzir\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Remover"
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
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Estado: N/A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Estado: Parado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Estado: Reproduzindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Estado: Reproduzindo (Em Loop)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Estado: Reproduzindo (Sequencial)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Reproduz várias faixas de áudio de uma só vez, mesmo dentro do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Ativar som de \"OK\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Ativar som de \"Cancelamento\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Ativar som de \"Aviso\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Ativar \"Música de fundo\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Ativar som de \"Rolagem\""
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Número máximo de usuários"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Número máximo de usuários suportados pelo RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Comportamento de Sondagem (Reinício necessário)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Influencia como a sondagem de entrada é feita no RetroArch. Definir como \"Mais cedo\" ou \"Mais tarde\" pode reduzir a latência, dependendo das suas configurações."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Influencia como a sondagem de entrada é feita dentro do RetroArch.\nMais cedo — Sondagem de entrada é executada antes do quadro ser processado.\nNormal — Sondagem de entrada é executada quando a sondagem é chamada.\nMais tarde — Sondagem de entrada é executada na primeira de chamada de entrada por quadro.\nDefinir como \"Mais cedo\" ou \"Mais tarde\" pode reduzir a latência, dependendo das suas configurações. É ignorada ao usar Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Remapear controles para este núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Substitua os vínculos da entrada com os vínculos definidos para o núcleo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Ordenar remapeamento por controle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Os remapeamento só serão aplicados ao controle ativo no qual foram salvos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Auto configuração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Configura automaticamente os controles que possuem um perfil, estilo Plug-and-Play."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Desativar teclas de atalho do Windows (requer reinício)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Mantenha as combinações das teclas de atalho dentro do aplicativo."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Selecionar Teclado Físico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Use este dispositivo como teclado físico e não como um controle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Se RetroArch identificar um teclado como algum tipo de controle, esta configuração pode ser usada para forçar RetroArch ajustar o dispositivo mal identificado como um teclado.\nIsso pode ser útil se você estiver tentando emular um computador em algum dispositivo Android TV e também possuir um teclado físico que pode ser anexado ao dispositivo."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Entrada do sensor auxiliar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Permite a entrada a partir dos sensores do acelerômetro, giroscópio e da iluminação caso seja compatível pelo hardware atual. Pode ter um impacto no desempenho e/ou aumentar o consumo de energia em algumas plataformas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Capturar mouse automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Ativa a captura do mouse ao focar no do aplicativo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Ative automaticamente o modo \"Foco do jogo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Sempre ative o modo \"Foco no Jogo\" ao iniciar e retomar o conteúdo. Quando definido como \"Detectar\", a opção será ativada se o núcleo atual implementar a funcionalidade de retorno de chamada do teclado na interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "LIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Detectar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Pausar conteúdo ao desconectar controle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Pausa o conteúdo quando um controle é desconectado. O botão \"Start\" despausa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Limite do eixo do botão da entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Define o quão longe um eixo deve ser inclinado para ativar o pressionamento de botão ao usar \"analógico para digital\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Zona morta do controle analógico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ignora o direcional analógico abaixo do valor da zona morta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Sensibilidade do controle analógico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Sensibilidade do Acelerômetro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Sensibilidade do Giroscópio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Ajusta a sensibilidade dos direcionais analógicos."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Ajustar a sensibilidade do Acelerômetro."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
  "Ajustar a sensibilidade do Giroscópio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Tempo limite para vincular"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Quantidade de segundos para aguardar até proceder para o próximo vínculo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Vincular (manter pressionado)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Quantidade de segundos para manter uma entrada para vinculá-la."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Pressione uma tecla do teclado, mouse ou controle"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Solte todas as teclas e os botões!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Tempo de espera"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Segurar"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Disparo Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "Desativado interrompe todas as operações do disparo turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Período do turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "O período em quadros durante o qual os botões com turbo ativado são pressionados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "Ciclo de Trabalho do Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "Número de quadros dentro do Período Turbo pelos quais os botões permanecem pressionados. Se esse número for igual ou maior que o Período Turbo, os botões nunca serão soltos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Meio Período"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Modo turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Selecione o comportamento geral do modo turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Clássico (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Botão Único (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Botão Único (Segurar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Modo clássico com operação de dois botões. Segure um botão e aperte o botão Turbo para ativar a sequência segure-solte.\nO botão Turbo pode ser assinado em Configurações/Entrada/Controles da Porta X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Modo clássico de alternância, operação com dois botões. Segure um botão e toque no botão Turbo para ativar o turbo para esse botão. Para desativar o turbo: segure o botão e pressione o botão Turbo novamente.\nA atribuição do botão Turbo pode ser configurada em Configurações/Entrada/Controles da Porta X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Modo de alternância. Aperte o botão Turbo uma vez para ativar a sequência segure-solte para o botão selecionado, aperte novamente para desligá-lo.\nO botão Turbo pode ser designado em Configurações/Entrada/Controles da Porta X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Modo segurar. A sequência de pressionar e soltar do botão padrão selecionado permanece ativa enquanto o botão Turbo estiver pressionado.\nA atribuição do Turbo pode ser configurada em Configurações/Entrada/Porta X Controles.\nPara emular a função de auto-disparo da era dos computadores domésticos, defina o Vincular e o Botão para o mesmo botão de disparo do joystick."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Botão Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "Turbo ativando o botão RetroPad. Vazio usa o botão específico da porta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Botão Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Botão turbo alvo no modo 'Botão Único'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Permitir Turbo para Direções do D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Se ativado, entradas direcionais digitais (também conhecidas como D-Pad ou 'hatswitch') podem usar turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Disparo Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Altera as configurações do disparo turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Resposta háptica e vibração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Altera as configurações da resposta háptica e a vibração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Controles do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Altera as configurações de controle do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Teclas de atalho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Altera as configurações e as atribuições das teclas de atalho, como ativar o menu durante o jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Vinculações do RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Altere como o RetroPad virtual é mapeado para um dispositivo de entrada físico. Se um dispositivo de entrada for reconhecido e configurado automaticamente corretamente, provavelmente não será necessário usar este menu.\nNota: para alterações específicas de núcleo nos controles, use o submenu 'Controles' do Menu Rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "O Libretro usa uma abstração de controle virtual chamada 'RetroPad' para se comunicar entre as interfaces (como o RetroArch) e os núcleos, e vice-versa. Este menu determina como o RetroPad virtual é mapeado para os dispositivos de entrada físicos e quais portas de entrada virtuais esses dispositivos ocupam.\nSe um dispositivo de entrada físico for reconhecido e configurado automaticamente corretamente, os usuários provavelmente não precisarão usar este menu e, para alterações específ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Controle da porta %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Altere como o RetroPad virtual está associado ao seu dispositivo de entrada físico nesta porta virtual."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Alterar mapeamentos de entrada específicos do core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Corrigir os desconectamentos (Android)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Solução alternativa para controladores que desconectam e reconectam. Impede que 2 jogadores usem controladores idênticos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_QUIT,
   "Confirmar sair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_QUIT,
   "Exige que o atalho para \"Sair\" seja pressionado duas vezes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_CLOSE,
   "Confirmar fechar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_CLOSE,
   "Exige que o atalho para \"Fechar contéudo\" seja pressionado duas vezes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_RESET,
   "Confirmar reiniciar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_RESET,
   "Exige que o atalho para \"Reiniciar contéudo\" seja pressionado duas vezes."
   )


/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibrar ao pressionar a tecla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Ativar vibração do dispositivo (para núcleos suportados)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Intensidade da vibração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Determine a intensidade dos efeitos da resposta háptica."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Controles de menu unificados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Utiliza os mesmos controles para o menu e jogo. Aplica-se ao teclado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Inverter Botões OK e Cancelar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Troca os botões de Confirmar e Cancelar. Desabilitado é o estilo japonês de botão. Habilitado é o estilo ocidental."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Inverter Botões de Rolagem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Trocar Botões de Rolagem. Desabilitar rolagem de 10 itens com L/R e alfabeticamente com L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Todos os usuários controlam o menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Permita que qualquer usuário possa controlar menu. Caso seja desativado, apenas o Usuário 1 poderá controlar o menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "Playlists com Clique Único"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "Pular o menu 'Executar' ao iniciar entradas da playlist. Pressione o Direcional enquanto segura OK para acessar o menu 'Executar'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "Permitir voltar a partir das tabs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "Retornar ao Menu Principal a partir das tabs/barra lateral ao pressionar Voltar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Aceleração de Rolagem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Velocidade máxima do cursor ao manter uma direção para rolar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Atraso de Rolagem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Atraso inicial em milissegundos ao segurar uma direção para rolar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Desativar botão de informações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Impede a função de informações do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Desativar botão de pesquisa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Impede a função de busca no menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Desativa o Analógico Esquerdo no Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Impede a entrada do analógico esquerdo no menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Desativa o Analógico Direito no Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Impede a entrada do analógico direito no menu. O analógico direito percorre as miniaturas nas listas de reprodução."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Ativar tecla de atalho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Quando atribuído, a tecla 'Ativar Hotkey' deve ser mantida pressionada antes que quaisquer outras hotkeys sejam reconhecidas. Permite que botões do controle sejam mapeados para funções de hotkey sem afetar a entrada normal. Atribuir o modificador apenas para controle não o exigirá para hotkeys do teclado, e vice-versa, mas ambos os modificadores funcionam para ambos os dispositivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Se esta tecla de atalho estiver vinculada ao teclado, botão do controle ou eixo do controle, todas as outras teclas de atalho serão desativadas, a menos que esta tecla de atalho também esteja sendo pressionada ao mesmo tempo.\nIsso é útil para implementações centradas no RETRO_KEYBOARD que verificam uma grande área do teclado, onde não é desejável que as teclas de atalho atrapalhem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Atraso da tecla de atalho (quadros)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Adiciona um atraso nos quadros antes que o comando normal seja bloqueada após pressionar e segurar a tecla atribuída como \"Tecla de atalho\". Permite que o comando normal da \"Tecla de atalho\" seja capturada quando for mapeada para outra ação (por exemplo, RetroPad \"Selecionar\")."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Mesclar Tipo de Dispositivo da Hotkey"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Bloqueia todas as hotkeys de ambos os tipos de dispositivos, teclado e controle, se qualquer um dos tipos tiver 'Ativar Hotkey' configurado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Atalhos Seguem o Jogador 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Os atalhos ficam vinculados à porta 1 do núcleo, mesmo que ela seja remapeada para um usuário diferente. Observação: os atalhos de teclado não funcionarão se a porta 1 for remapeada para qualquer usuário acima de 1 (pois a entrada do teclado é do usuário 1)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Ativar Menu (Combo de Comandos)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Combinação de botões do controle para alternar o menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Ativar menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Alterna a exibição atual entre o menu e o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Sair (atalho do controle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "A combinação dos botões do controle para sair do RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Sair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Fecha o RetroArch, garantindo que todos os dados salvos e arquivos de configuração sejam liberados para o disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Fechar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Fecha o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Reiniciar (conteúdo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Reinicia o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Avanço rápido (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Alterna entre avanço rápido e velocidade normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Avanço rápido (segurar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Habilita o avanço rápido enquanto a tecla é pressionada. O conteúdo é executado na velocidade normal quando a tecla é liberada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Câmera lenta (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Alterna entre câmera lenta e velocidade normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Câmera lenta (segurar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Habilita a câmera lenta enquanto a tecla é pressionada. O conteúdo é executado na velocidade normal quando a tecla é liberada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Retroceder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Retrocede o conteúdo atual enquanto a tecla é pressionada. O 'Suporte a Retrocesso' deve estar ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Pausar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Alterna o conteúdo entre os estados pausado e não pausado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Avanço do Quadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Avança o conteúdo por um quadro quando pausado."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Silenciar áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Alterna a saída de áudio entre ligada e desligada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Aumentar volume"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Aumenta o nível do volume da saída de áudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Diminuir volume"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Diminui o nível do volume da saída de áudio."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Carregar jogo salvo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Carrega o ponto de save do slot atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Salva o ponto de save no slot atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Próximo Slot de Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Incrementa o índice do slot de ponto de save atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Slot Anterior do Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Decrementa o índice do slot de ponto de save atualmente selecionado."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Ejetar Disco (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Se a bandeja de disco virtual estiver fechada, ela abre e remove o disco carregado. Caso contrário, insere o disco atualmente selecionado e fecha a bandeja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Próximo Disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Incrementa o índice do disco selecionado atualmente e realiza a inserção atrasada se a bandeja de disco virtual estiver fechada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Disco Anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Decrementa o índice do disco selecionado atualmente e realiza a inserção atrasada se a bandeja de disco virtual estiver fechada."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Sombreados (ativar/desativar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Liga/desliga o sombreador atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Shaders (Manter pressionado)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Mantém o shader selecionado ativado/desativado enquanto a tecla estiver pressionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Próximo sombreador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Carrega e aplica o arquivo de predefinição do shader anterior na raiz do diretório \"Shader de vídeo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Sombreador anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Carrega e aplica o arquivo de predefinição do shader anterior na raiz do diretório \"Shader de vídeo\"."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Trapaças (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Alterna entre ligada e desligada a trapaça atualmente selecionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Próximo índice de trapaça"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Aumenta o índice de trapaça selecionado atualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Índice de trapaça anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Diminui o índice de trapaça selecionado atualmente."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Capturar tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Salva uma imagem do conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Gravação (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Inicia ou interrompe a gravação da sessão atual em um arquivo de vídeo local."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Transmissão (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Inicia ou interrompe o transmissão da sessão atual para uma plataforma de vídeo online."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Reproduzir Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Reproduzir arquivo de replay do slot selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Gravar replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Gravar arquivo de replay no slot selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Interromper Gravação/Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Interrompe gravação/reprodução do replay atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Salvar Checkpoint do Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Salva um checkpoint no replay atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Checkpoint do Replay Anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Retrocede o replay para o checkpoint salvo anterior, automática ou manualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Próximo Checkpoint"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Avança o replay para o próximo checkpoint salvo automática ou manualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Próximo Slot de Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Incrementa o índice do slot de replay atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Slot de Replay Anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Decrementa o índice do slot de replay atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "Disparo Turbo (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Alterna o disparo turbo entre ligado/desligado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Captura do mouse (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Captura ou libera o mouse. Quando capturado, o cursor do sistema fica oculto e confinado à janela de exibição do RetroArch, melhorando a entrada relativa do mouse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Foco do jogo (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Habilita ou desabilita o modo \"Foco do jogo\". Quando o conteúdo está focado, as teclas de atalho são desabilitadas (a entrada completa do teclado é passada para o núcleo em execução) e o mouse é capturado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Tela cheia (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Alterna entre os modos de exibição em tela cheia e em janela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Menu tradicional (ativar/desativar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Abre a interface do usuário de desktop WIMP (janelas, ícones, menus, ponteiro)."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Sincronizar com a Taxa de Quadros Exata do Conteúdo (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Alterna a sincronização com a taxa de quadros exata do conteúdo entre ligado/desligado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Antecipação de Execução (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Ativa/desativa a Antecipação de Execução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Antecipação de Quadros (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Alterna a Antecipação de Quadros entre ligado/desligado."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Mostrar FPS (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Alterna o indicador de estado dos \"quadros por segundo\" entre ligado e desligado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Mostrar estatísticas técnicas (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Liga ou desliga a exibição das estatísticas técnicas na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Sobreposição do Teclado (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Alterna a sobreposição do teclado entre ligado/desligado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Próxima sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Muda para a próxima disposição disponível da sobreposição atualmente ativa na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Serviço de IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Captura uma imagem do conteúdo atual e depois traduz e/ou lê em voz alta qualquer texto na tela. O \"Serviço de IA\" deve estar ativo e configurado."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Ping da Netplay (ativar/desativar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Liga ou desliga o contador de latência da sala atual de jogo em rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Hospedagem de jogo em rede (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Alterna a hospedagem da Netplay entre ligada e desligada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Modo espectador/jogador da Netplay (ativar/desativar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Alterna a sessão atual de jogo entre os modos \"espectador\" e \"jogador\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Bate-papo da Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Envia uma mensagem de bate-papo para a sessão atual da Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Desvanecer Chat no Netplay (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Ativa ou desativa o desvanecimento das mensagens do bate-papo da Netplay."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Tipo de Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Especifica o tipo de controle emulado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Tipo de analógico para digital"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Usa o analógico especificado para entrada do D-Pad. Modos 'Forçados' ignoram a entrada analógica nativa do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Mapeia o direcional analógico especificado para entrada do D-Pad.\nSe o núcleo tiver suporte nativo para analógico, o mapeamento do D-Pad será desativado, a menos que a opção '(Forçado)' seja selecionada.\nSe o mapeamento do D-Pad for forçado, o núcleo não receberá entrada analógica do direcional especificado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Índice de Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "O controle físico, conforme reconhecido pelo RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Dispositivo Reservado para Este Jogador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Este controle será alocado para este jogador, de acordo com o modo de reserva."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Sem Reserva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Preferido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Reservado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Tipo de Reserva de Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Preferencial: se o dispositivo especificado estiver presente, será alocado para este jogador. Reservado: nenhum outro controle será alocado para este jogador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Porta mapeada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Especifica qual porta do núcleo receberá a entrada da porta de controle da interface %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Definir todos os controles"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Atribui todas as direções e botões, um após o outro, na ordem em que aparecem neste menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Restaurar controles padrão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Limpa configurações de vinculação de entrada para seus valores padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Salvar perfil de controle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Salva um arquivo de autoconfiguração que será aplicado automaticamente sempre que este controle for detectado novamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Índice de mouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "O mouse físico, conforme reconhecido pelo RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Botão B (baixo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Botão Y (esquerda)"
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
   "Direcional digital para cima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Direcional digital para baixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Direcional digital esquerdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Direcional digital direito"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Botão A (direita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Botão X (topo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Botão L (ombro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Botão R (ombro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Botão L2 (gatilho)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Botão R2 (gatilho)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Botão L3 (polegar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Botão R3 (polegar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Analógico esquerdo X+ (direita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Analógico esquerdo X- (esquerda)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Analógico esquerdo Y+ (baixo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Analógico esquerdo Y- (cima)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Analógico direito X+ (direita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Analógico direito X- (esquerda)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Analógico direito Y+ (baixo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Analógico direito Y- (cima)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Gatinho da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Recarregar pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Aux A da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Aux B da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Aux C da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Start da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Select da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Direcional digital cima da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Direcional digital baixo da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Direcional digital esquerdo da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Direcional digital direito da pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Disparo Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOLD,
   "Segurar"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Antecipação de Execução Indisponível]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "O núcleo atual é incompatível com a antecipação de execução devido à falta de suporte determinístico a save state."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
   "Antecipação de Execução"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Número de Quadros para Antecipação de Execução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "O número de quadros a serem processados antecipadamente. Causa problemas na jogabilidade, como trepidações, se o número de quadros de atraso (lag frames) internos do jogo for excedido."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Executa lógica adicional do núcleo para reduzir a latência. O Modo Instância Única roda até um quadro futuro, então recarrega o estado atual. O Modo Segunda Instância mantém uma instância do núcleo só com vídeo em um quadro futuro para evitar problemas de estado de áudio. O Modo Antecipação de Quadros repete quadros passados com nova entrada quando necessário, para eficiência."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Executa lógica adicional do núcleo para reduzir a latência. Instância Única roda até um quadro futuro, então recarrega o estado atual. Antecipação de Quadros repete quadros passados com nova entrada quando necessário, para eficiência."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Modo Instância Única"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Modo Segunda Instância"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Modo Antecipação de Quadros"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Ocultar Avisos de Antecipação de Execução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Oculta a mensagem de aviso que aparece ao usar a Antecipação de Execução quando o núcleo não suporta save states."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Número de Antecipação de Quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Número de frames a serem repetidos. Pode causar problemas na jogabilidade, como tremedeira, se o número de frames de atraso interno ao jogo for ultrapassado."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Contexto compartilhado de hardware"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Concede aos núcleos renderizados por hardware o seu contexto próprio. Evita ter que assumir as alterações do estado do hardware entre os quadros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Permitir que os núcleos alterem o driver de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Permite que os núcleos alternem para um driver de vídeo diferente do que o usado atualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Ao fechar o núcleo, carregar um núcleo falso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Alguns núcleos têm um recurso de desligamento. Ao carregar um núcleo falso impedirá o RetroArch de desligar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Alguns núcleos podem ter um recurso de desligamento. Se esta opção ficar desativada, selecionar o procedimento de desligamento fará com que o RetroArch seja encerrado.\nAtivar esta opção carregará um núcleo fictício para que permaneçamos dentro do menu e o RetroArch não seja encerrado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Iniciar um núcleo automaticamente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Categorias"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Permite que os núcleos organizem visual as configurações em submenus baseados em categorias. \nOBSERVAÇÃO: os núcleos devem ser recarregados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Arquivos de informação de cache do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Mantem um cache local persistente da informação principal instalada. Reduz significativamente o tempo de carregamento em plataformas com acesso lento ao disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Ignorar Recursos de Save States da Informação do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Especifica se deve ignorar as capacidades de salvamento de estado das informações do núcleo, permitindo testar recursos relacionados (antecipação de quadros, retrocesso, etc)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Sempre recarregar o núcleo na execução de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Reinicie o RetroArch ao iniciar o conteúdo, mesmo que o núcleo já esteja carregado. Isto pode melhorar a estabilidade do sistema, ao custo do aumento do tempo de carregamento."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Permitir rotação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Permite que os núcleos definam a rotação. Quando desabilitado, as requisições de rotação são ignoradas. Útil para configurações que giram manualmente a tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Gerenciar núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Executa tarefas de manutenção offline nos núcleos instalados (cópia de segurança, restauração, exclusão etc.) e visualiza as informações principais."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Gerenciar núcleos"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Instala ou desinstala núcleos disponíveis no Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Instalar núcleo"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Desinstalar núcleo"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Mostrar \"Gerenciar núcleos\""
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Mostra a opção \"Gerenciar núcleos\" no menu principal."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Instalando núcleo: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "O núcleo será desinstalado ao fechar o RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "O núcleo está sendo baixado"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Salvar configuração ao sair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Salva as alterações no arquivo de configuração ao sair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Salva alterações no arquivo de configuração ao sair. Útil para mudanças feitas no menu. Sobrescreve o arquivo de configuração, inclusões (#include) e comentários não são preservados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Salvar arquivos de remapeamento ao sair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Salva as alterações em qualquer arquivo de remapeamento de entrada ativo ao fechar o contéudo ou sair do RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Carregar automaticamente configurações específicas do núcleo por conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Carrega a configuração de núcleos personalizada por padrão na inicialização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Carrega automaticamente arquivos de personalização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Carrega a configuração personalizada na inicialização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Carrega automaticamente arquivos de remapeamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Carrega os controles personalizados na inicialização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Carregar Arquivos Iniciais de Índice de Disco Automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Alterar para o último disco usado ao iniciar conteúdo multi-discos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Carregar automaticamente predefinições de shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Usar arquivo global de configurações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Salve todas as configurações do núcleo em um arquivo de configuração comum (retroarch-core-options.cfg). Quando desativado, as configurações de cada núcleo são salvas em uma pasta/arquivo específico do núcleo, separado no diretório \"Config\" do RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Arquivo de Save: Organizar em Pastas por Pome do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Ordena os arquivos de dados da memória do jogo em pastas com o nome do núcleo utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Save State: Organizar em Pastas por Nome do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Organiza save states em pastas nomeadas conforme o núcleo utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Arquivo de Save: Organizar em Pastas por Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Ordena os arquivos de dados da memória do jogo em pastas com o nome do diretório em que o conteúdo está localizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Save State: Organizar em Pastas por Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Organiza save states em pastas nomeadas conforme o diretório onde o conteúdo está localizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Arquivo de Save: Não Sobrescrever SaveRAM ao Carregar Save State"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Impede que a SaveRAM seja sobrescrita ao carregar pontos de save. Pode, potencialmente, causar falhas nos jogos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Arquivo de Save: Intervalo de Salvamento Automático da SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Salve automaticamente o SaveRAM não volátil em um intervalo regular (em segundos)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Salva automaticamente a SRAM não volátil em intervalos regulares. Isso está desativado por padrão, a menos que configurado de outra forma. O intervalo é medido em segundos. O valor 0 desativa o salvamento automático."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Replay: Intervalo de Checkpoint"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Marca automaticamente o estado do jogo durante a gravação do replay em um intervalo regular (em segundos)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Salva automaticamente o estado do jogo durante a gravação de replay em intervalos regulares. Isso está desativado por padrão, a menos que configurado de outra forma. O intervalo é medido em segundos. O valor 0 desativa a gravação de checkpoints."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE,
   "Define se os checkpoints armazenados em replays devem ser desserializados durante a reprodução normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "Replay: Desserializar Checkpoint"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "Define se os checkpoints armazenados em replays devem ser desserializados durante a reprodução normal. Deve ser definido como verdadeiro para a maioria dos núcleos, mas alguns podem apresentar comportamento instável ao desserializar o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Save State: Incrementar Índice Automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Antes de criar um ponto de save, o índice do ponto de save é incrementado automaticamente. Ao carregar o conteúdo, o índice será definido como o maior índice existente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Replay: Incrementar Índice Automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Antes de criar um replay, o índice de replay é incrementado automaticamente. Ao carregar um conteúdo, o índice será definido como o maior índice existente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Save State: Máximo de Incrementos Automáticos a Manter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Limita o número de save states que serão criados quando 'Incrementar Índice Automaticamente' estiver ativado. Se o limite for excedido ao salvar um novo state, o state existente com o índice mais baixo será excluído. O valor '0' significa que states ilimitados serão registrados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Replay: Máximo de Incrementos Automáticos a Manter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Limita o número de replays que serão criados quando 'Incrementar Índice Automaticamente' estiver ativado. Se o limite for excedido ao gravar um novo replay, o replay existente com o índice mais baixo será excluído. O valor '0' significa que replays ilimitados serão registrados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Save State: Salvamento Automático"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Cria automaticamente um save state quando o conteúdo é fechado. Este save state é carregado na inicialização se o 'Carregamento Automático' estiver ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Save State: Carregamento Automático"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Carrega automaticamente o ponto de save automático na inicialização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Save State: Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Exibe miniaturas de save states."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Arquivo de Save: Compressão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Salve os arquivos SaveRAM não voláteis em um formato compactado. Reduz drasticamente o tamanho do arquivo às custas do (insignificante) aumento do tempo de salvamento/carregamento.\nAplica-se apenas aos núcleos que permitem salvar através da interface libretro SaveRAM padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Save State: Compressão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Grava arquivos de ponto de save em formato compactado. Reduz drasticamente o tamanho dos arquivos, em troca de tempos maiores para salvamento/carregamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Arquivo de Save: Gravar no Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Usar diretório do conteúdo como diretório de arquivos de save."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Save State: Gravar no Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Usa o diretório do conteúdo como diretório de ponto de save."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Salvar arquivos de sistema no diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Usa o diretório de conteúdo como diretório de Sistema/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Captura de Tela: Organizar em Pastas por Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Ordena as capturas de tela em pastas com o nome do diretório no qual o conteúdo está localizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Captura de Tela: Gravar no Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Usa o diretório de conteúdo como diretório de captura de tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Captura de Tela: Usar GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Captura a tela com shader de GPU caso esteja disponível."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Salvar registro de tempo de execução (por núcleo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Mantém um registro de quanto tempo cada item está sendo executado separado por núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Salvar registro de tempo de execução (agregar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Mantém um registro de quanto tempo cada item está sendo executado agregando todos os núcleos."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Verbosidade do registro de eventos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Habilita ou desabilita o registro de eventos no terminal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Nível de Registro da Interface"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Define o nível de registro para a interface. Se um nível do registro emitido pela interface estiver abaixo desse valor, este será ignorado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Nível de registro de eventos do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Define o nível de registro de eventos para os núcleos. Se o nível do registro enviado por um núcleo for abaixo deste valor, este é ignorado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Define o nível de log para os núcleos libretro (GET_LOG_INTERFACE). Se um nível de log emitido por um núcleo libretro for inferior ao nível de libretro_log, ele será ignorado. Logs DEBUG são sempre ignorados, a menos que o modo verbose seja ativado (--verbose).\nDEBUG = 0\nINFO  = 1\nWARN  = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Depuração)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Aviso)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Erro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Registrar em arquivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Redireciona as mensagens do registro de eventos do sistema para um arquivo. Requer \"Verbosidade do registro de eventos\" para ativar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Arquivos de registro com data e hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Ao fazer o registro de eventos num arquivo, redirecione a saída de cada sessão do RetroArch para um novo arquivo com o registro da data e da hora. Caso seja desativado, irá substituir o arquivo do registro de eventos sempre que o RetroArch for reiniciado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Contadores de desempenho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Contadores de desempenho para o RetroArch e núcleos. Os dados obtidos podem ser úteis em definir gargalos e ajustar o desempenho do sistema e do aplicativo."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Mostrar arquivos e pastas ocultas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Mostrar arquivos e diretórios ocultos no Navegador de Arquivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrar extensões desconhecidas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrar arquivos exibidos no Navegador de Arquivos por extensões suportadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filtrar por núcleo atual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Filtrar arquivos exibidos no Navegador de Arquivos pelo núcleo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Lembrar do último diretório usado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Abrir o Navegador de Arquivos no último local usado ao carregar conteúdo a partir do Diretório Inicial. Obs.: O local será redefinido para o padrão ao reiniciar o RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Sempre Sugerir Núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Sugerir núcleos disponíveis mesmo quando um núcleo estiver carregado manualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Utilizar o reprodutor de mídia integrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Utilizar o visualizador de imagem integrado"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Retroceder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_SETTINGS,
   "Altera as configurações de retrocesso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Contador de duração de quadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Altera as configurações influenciando o contador de tempo do quadro.\nAtivo somente quando os vídeos em paralelo estiverem desativados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Taxa de avanço rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Taxa máxima em que o conteúdo será executado quando utilizado o Avanço Rápido (ex: 5.0x para conteúdos em 60fps = 300 fps máx). Se definido como 0.0x, a taxa de Avanço Rápido é ilimitada (sem FPS máx)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Taxa máxima na qual o conteúdo será executado ao usar o avanço rápido. (Ex.: 5.0 para conteúdo a 60 fps => limite de 300 fps).\nO RetroArch irá pausar momentaneamente para garantir que a taxa máxima não seja excedida. Não confie nesse limite como sendo perfeitamente preciso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Avançar pulo de quadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Pular frames de acordo com a taxa de avanço rápido. Isso economiza energia e permite o uso de limitadores de frames de terceiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Taxa de câmera lenta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "A taxa que o conteúdo será reproduzido ao usar a câmera lenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Limitar Taxa de Quadros no Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Garante que a taxa de quadros seja limitada enquanto estiver dentro do menu."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Suporte a Retrocesso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Retorna a um ponto anterior na jogabilidade recente. Isso causa um impacto severo no desempenho ao jogar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Quadros de Retrocesso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "O número de quadros para retroceder por etapa. Valores mais altos aumentam a velocidade do retrocesso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Tamanho do Buffer de Retrocesso (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "A quantidade de memória (em MB) para reservar para o buffer de retrocesso. Aumentar este valor aumentará o histórico do retrocesso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Etapa do Tamanho do Buffer de Retrocesso (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Cada vez que o valor do tamanho do buffer de retrocesso é aumentado ou diminuído, ele mudará por esta quantia."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Reiniciar Após o Avanço Rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Reinicia o contador de tempo do quadro após o avanço rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Reiniciar Após Carregar o Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Reiniciar o contador de tempo do quadro após carregar um jogo salvo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Reiniciar Após o Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Reinicia o contador de tempo de frame após salvar um ponto de save."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Qualidade da gravação"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Personalizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Baixa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Média"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Alta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Sem perda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM (rápido)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM (alta qualidade)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Configuração de gravação personalizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Threads de gravação"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Usar gravação pós-filtro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Capture a imagem depois que os filtros (mas não os shaders) forem aplicados. O seu vídeo ficará tão bonito quanto o que você vê na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Usar gravação da GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Se disponível, grave o material gerado através do shader da GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Modo de transmissão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Personalizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Qualidade da transmissão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Baixa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Média"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Alta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Configuração de transmissão personalizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Título da transmissão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL da transmissão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Porta da transmissão UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Sobreposição na tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Ajusta as molduras e os controles na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Layout de Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Ajustar Layout de Vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Notificações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Ajusta as notificações na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Visibilidade das notificações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Alterna a visibilidade dos tipos específicos de notificações."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Exibir sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "As sobreposições são usadas para aplicar as molduras exibir os controles na tela."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Mostrar sobreposição por trás do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Mostra a sobreposição do menu atrás em vez de mostrar na frente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Ocultar sobreposição no menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Oculta a sobreposição enquanto estiver dentro do menu e exibe novamente ao sair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Ocultar sobreposição quando controle está conectado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Oculta a sobreposição quando um controle físico está conectado na porta 1, e volta a exibir a sobreposição ao desconectar o controle."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Oculta a sobreposição quando um controle é conectado na porta 1. A sobreposição não será restaurada automaticamente quando o controle for desconectado."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Mostrar entradas na sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Mostra as entradas registradas na sobreposição. \"Tocados\" serão destacados os elementos pressionados ou clicados na sobreposição. Físico (controle) será destacada a entrada real transmitida para os núcleos, geralmente, de um controle ou teclado conectado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Tocado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Físico (controle)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Mostrar entradas da porta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Selecione a porta do dispositivo de entrada que será monitorada quando a opção \"Mostrar entradas na sobreposição\" estiver configurada como \"Físico (controle)\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Mostrar cursor do mouse na sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Mostra o cursor do mouse ao usar uma sobreposição na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Auto-rotação da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Se suportado pela sobreposição atual, girar automaticamente a disposição para ajustar com a orientação e proporção da tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Redimensionar automaticamente a sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Ajusta automaticamente a escala da sobreposição e do espaçamento dos elementos da interface para que coincida com a proporção da tela. Produz melhores resultados com as sobreposições dos controles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Sensibilidade diagonal do Direcional"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Ajuste o tamanho das zonas diagonais. Ajuste a 100% para a simetria de 8 vias."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Sensibilidade da sobreposição ABXY"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Ajuste o tamanho das zonas de sobreposição. Ajuste para 100% para a simetria de 8 vias."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Zona de Recentralização do Analógico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "A entrada do direcional analógico será relativa ao primeiro toque, se pressionado dentro desta zona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Sobreposição"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Carrega automaticamente sobreposição favorita"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Prefira carregar sobreposições com base no nome do sistema antes de retornar à predefinição padrão. Será ignorado se uma substituição for definida para a predefinição de sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Opacidade da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Opacidade de todos os elementos de interface da sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Predefinição da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Selecionar uma sobreposição no Navegador de Arquivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Modo paisagem) Escala da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Dimensiona todos os elementos da interface na sobreposição ao usar uma orientação de tela no modo retrato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Modo paisagem) Proporção da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Aplica um fator de correção de proporção da tela à sobreposição ao usar uma orientação de tela no modo paisagem. Os valores positivos aumentam (enquanto os valores negativos diminuem) a largura da sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Modo paisagem) Separação horizontal da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Caso seja compatível pela configuração atual, ajusta o espaçamento entre os elementos da interface das metades da esquerda e da direita quando usar as orientações do modo retrato em uma sobreposição. Os valores positivos aumentam (enquanto os valores negativos diminuem) a separação de ambas as metades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Modo paisagem) Separação vertical da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Caso seja compatível pela configuração atual, ajusta o espaçamento entre os elementos da interface das metades de cima e das de baixo quando usar as orientações do modo retrato em uma sobreposição. Os valores positivos aumentam (enquanto os valores negativos diminuem) a separação de ambas as metades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Modo paisagem) Deslocar X da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Compensa a sobreposição no eixo horizontal ao usar uma orientação de tela no modo retrato. Os valores positivos deslocarão a sobreposição para a direita e os valores negativos para a esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Modo paisagem) Deslocar Y da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Compensa a sobreposição no eixo vertical ao usar uma orientação de tela no modo retrato. Os valores positivos deslocarão a sobreposição para cima e os valores negativos para baixo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Modo retrato) Escala da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Dimensiona todos os elementos da interface na sobreposição ao usar uma orientação de tela no modo retrato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Modo retrato) Proporção da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Aplica um fator de correção da proporção da tela à sobreposição ao usar uma orientação de tela no modo retrato. Os valores positivos aumentam (enquanto os valores negativos diminuem) a altura da sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Modo retrato) Separação horizontal da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Caso seja compatível pela configuração atual, ajusta o espaçamento entre os elementos da interface das metades da esquerda e da direita quando usar as orientações do modo retrato em uma sobreposição. Os valores positivos aumentam (enquanto os valores negativos diminuem) a separação de ambas as metades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Modo retrato) Separação vertical da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Caso seja compatível pela configuração atual, ajusta o espaçamento entre os elementos da interface das metades de cima e das de baixo quando usar as orientações do modo retrato em uma sobreposição. Os valores positivos aumentam (enquanto os valores negativos diminuem) a separação de ambas as metades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Modo retrato) Deslocar X da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Compensa a sobreposição no eixo horizontal ao usar uma orientação de tela no modo retrato. Os valores positivos deslocarão a sobreposição para a direita e os valores negativos para a esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Modo retrato) Deslocar Y da sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Compensa a sobreposição no eixo vertical ao usar uma orientação de tela no modo retrato. Os valores positivos deslocarão a sobreposição para cima e os valores negativos para baixo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Sobreposição de Teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Selecionar e ajustar uma sobreposição de teclado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Ativar o Lightgun, Mouse e Ponteiro na Sobreposição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Usar qualquer entrada de toque que não pressione controles da sobreposição para criar entrada de dispositivo de apontar para o núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Sobreposição da Pistola de Luz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Configura a entrada da pistola de luz enviada pela sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Sobreposição do Mouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Configura a entrada do mouse enviada pela sobreposição. Nota: Toques com 1, 2 e 3 dedos enviam cliques dos botões esquerdo, direito e do meio, respectivamente."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Predefinição da Sobreposição de Teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Seleciona uma sobreposição do teclado no Navegador de Arquivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Sobreposição do Teclado com Escala Automática"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Ajusta a sobreposição do teclado para sua proporção original. Desative para esticar na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Opacidade da Sobreposição do Teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Opacidade de todos os elementos da Interface da sobreposição do teclado."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Porta da Pistola de Luz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Define a porta do núcleo que receberá entrada da pistola de luz da sobreposição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Qualquer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Disparar ao Tocar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Envia o comando de disparo junto com a entrada do ponteiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Atraso no Disparo (frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Atrasa o disparo para permitir que o cursor se mova. Esse atraso também serve para aguardar a contagem correta de toques múltiplos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Entrada Multitoque (2 dedos)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Seleciona a entrada a ser enviada quando dois ponteiros estiverem na tela. O Atraso do Gatilho deve ser diferente de zero para distinguir das outras entradas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Entrada Multitoque (3 dedos)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Seleciona a entrada a ser enviada quando três ponteiros estiverem na tela. O Atraso do Gatilho deve ser diferente de zero para distinguir das outras entradas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Entrada Multitoque (4 dedos)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Seleciona a entrada a ser enviada quando quatro ponteiros estiverem na tela. O Atraso do Gatilho deve ser diferente de zero para distinguir das outras entradas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Permitir Fora da Tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Permitir mira fora dos limites. Desative para restringir a mira fora da tela à borda dentro dos limites."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Velocidade do mouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Ajusta a velocidade de movimento do cursor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Toque Longo para Arrastar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Toque e segure na tela para começar a segurar um botão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Limite de Toque Longo (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Ajusta o tempo necessário de toque para ser considerado um toque longo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Toque Duplo para Arrastar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Toque duas vezes na tela para começar a segurar um botão no segundo toque. Adiciona latência aos cliques do mouse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Limite de Toque Duplo (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Ajusta o intervalo permitido entre os toques ao detectar um toque duplo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Limite de Deslize"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "Entrada Alternativa de 2 Toques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "Usa o segundo toque como um botão do mouse enquanto controla o cursor."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Ajusta a margem de desvio permitida ao detectar um toque longo ou simples. Expresso como uma porcentagem da menor dimensão da tela."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Habilitar notificações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Mostra as mensagens na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Widgets gráficos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Usa animações decoradas, notificações, indicadores e controles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Dimensionar widgets gráficos automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Redimensiona automaticamente notificações, indicadores e controles decorados com base na escala atual do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Substituição da escala dos widgets gráficos (tela cheia)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Aplica uma substituição manual da escala ao desenhar os widgets na tela em modo de tela inteira. Se aplica apenas quando \"Dimensionar widgets gráficos automaticamente\" está desativado. Pode ser usado para aumentar ou diminuir o tamanho das notificações, indicadores e controles decorados independentemente do próprio menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Substituição da escala dos widgets gráficos (modo janela)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Aplica uma substituição manual da escala ao desenhar os widgets na tela no modo janela. Aplica-se apenas quando a \"Escala automática nos widgets gráficos\" está desativado. Pode ser usado para aumentar ou diminuir o tamanho das notificações, indicadores e controles decorados independentemente do próprio menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Exibir Taxa de Quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Exibe a quantidade de quadros por segundo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Intervalo de Atualização da Taxa de Quadros (Em Quadros)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "A exibição da taxa de quadros será atualizada no intervalo definido em quadros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Exibir contagem de quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Exibe a contagem atual dos quadros na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Exibir estatísticas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Exibe as estatísticas técnicas na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Exibir uso da memória"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Exibe a quantidade total e a quantidade utilizada da memória no sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Intervalo de atualização do uso de memória (em quadros)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "O indicador da quantidade da memória utilizada será atualizada no intervalo definido em quadros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Mostrar latência da Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Exibe a latência (ping) da sala atual da Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Notificação de inicialização ao \"Carregar conteúdo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Mostra uma breve animação como retorno ao carregar um conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Notificações da entrada das conexões (configuração automática)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Notificações de Falha de Entrada (Autoconfiguração)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Notificações do Códigos de Trapaças"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Exibe uma mensagem na tela quando códigos de trapaça são aplicados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Notificações de patch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Exibe uma mensagem na tela ao aplicar patchs nas ROMs em tempo real."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Exibe uma mensagem na tela ao conectar ou desconectar os dispositivos da entrada."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Exibe uma mensagem na tela quando os dispositivos de entrada não puderem ser configurados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Notificações da recarga do remapeamento da entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Exibe uma mensagem na tela ao carregar os arquivos do remapeamento da entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Notificações da recarga das configurações que foram substituídas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Exibe uma mensagem na tela ao carregar os arquivos de configuração da personalização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Notificações ao restaurar o disco inicial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Exibe uma mensagem na tela ao restaurar automaticamente o último disco usado ao lançar um conteúdo com vários discos usando as listas de reprodução M3U."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Notificação de disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Exibe uma mensagem na tela ao inserir ou ejetar discos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Notificação de salvamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Exibe uma mensagem ao salvar ou carregar salvamento."
   )
MSG_HASH( /* FIXME: Rename config key and msg hash */
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Notificações de Controle de Quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Mostra um indicador na tela quando o avanço rápido, a câmera lenta ou o retrocesso estiverem ativos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Notificação de captura de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Exibe uma mensagem na tela ao fazer uma captura de tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Persistência da notificação de captura de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Define a duração da mensagem na captura de tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Rápido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Muito rápido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Instantâneo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Efeito de flash ao capturar tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Exibe um efeito piscante branco na tela com a duração desejada ao fazer uma captura de tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "LIGADO (normal)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "LIGADO (rápido)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Notificações da taxa de atualização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Exibe uma mensagem na tela ao definir a taxa de atualização."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Notificações adicionais de jogo em rede"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Exibe mensagens não essenciais do jogo em rede na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Limitar as notificações ao menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Exibe as notificações somente quando o menu tiver sido aberto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Fonte das notificações na tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Seleciona a fonte para as notificações na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Tamanho da notificações na tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Especifique o tamanho da fonte em pontos. Quando widgets são usados, esse tamanho afeta apenas a exibição de estatísticas na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Posição X da notificação na tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Especifique a posição personalizada no eixo X para o texto na tela. 0 é a borda esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Posição Y da notificação na tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Especifique a posição personalizada no eixo Y para o texto na tela. 0 é a borda inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Notificação em cor vermelha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Define o valor do vermelho da cor do texto do OSD. Valores válidos estão entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Notificação em cor verde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Define o valor do verde da cor do texto do OSD. Valores válidos estão entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Notificação em cor azul"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Define o valor do azul da cor do texto do OSD. Valores válidos estão entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Ativar \"Notificação de fundo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Ativa uma cor de fundo para o OSD."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Notificação de fundo em cor vermelha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Define o valor do vermelho da cor de fundo do OSD. Valores válidos estão entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Notificação de fundo em cor verde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Define o valor do verde da cor de fundo do OSD. Valores válidos estão entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Notificação de fundo em cor azul"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Define o valor do azul da cor de fundo do OSD. Valores válidos estão entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Opacidade da notificação de fundo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Define a opacidade da cor de fundo do OSD. Valores válidos estão entre 0,0 e 1,0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Aparência"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Altera as configurações da aparência da tela do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Visibilidade dos itens do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Alterna a visibilidade dos itens do menu no RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Pausar conteúdo ao abrir menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Pausa o conteúdo quando o menu é aberto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Pausar conteúdo ao focar outra janela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Pausa o conteúdo quando o RetroArch não está em primeiro plano."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Encerrar ao fechar o conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Encerra automaticamente o RetroArch ao fechar o conteúdo. O \"CLI\" só fecha quando o conteúdo é executado através da linha de comando."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Retomar Conteúdo Após Usar Save States"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Fecha automaticamente o menu e continua o conteúdo atual após salvar ou carregar um jogo salvo. Desativar isso pode melhorar o desempenho ao salvar um jogo em dispositivos muito lentos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Retornar ao contéudo ao alterar o disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Fecha o menu automaticamente e retoma o conteúdo atual depois de selecionar \"Inserir disco\" ou \"Carregar novo disco\" no menu \"Controle de disco\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navegação retorna ao início"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Volta ao início ou final se o limite da lista for alcançado horizontalmente ou verticalmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Mostrar configurações avançadas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Mostre as configurações avançadas para usuários experientes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Modo quiosque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Protege a configuração escondendo todas as configurações relacionadas à configuração."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Senha para desativar modo quiosque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Define a senha para desativar o modo quiosque. A opção \"Desativar modo quiosque\" aparece na aba \"Informações\" no \"Menu principal\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Suporte para mouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Permite que o menu seja controlado através de um mouse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Suporte para toque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Permite que o menu seja controlado através de uma tela sensível ao toque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Paralelismo de tarefas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Executar tarefas em linhas de processamento paralelas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Tempo de espera do protetor de tela do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Enquanto o menu estiver ativo, um protetor de tela será exibido após o período especificado de inatividade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Menu da animação do protetor de tela do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Habilita um efeito de animação enquanto o protetor de tela do menu estiver ativo. Tem um pequeno impacto no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Neve"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Estrelas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "Vórtice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Velocidade da animação do protetor de tela do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Ajusta a velocidade do efeito de animação do protetor de tela do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Desativar composição da área de trabalho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Os gerenciadores da janela usam uma composição para aplicar os efeitos visuais, para detectar as janelas não estejam responsivas, entre outras coisas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Desativa a composição à força. A desativação é válida apenas no Windows Vista/7 por enquanto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Assistente de interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Iniciar o assistente de interface na inicialização"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Inicia o driver auxiliar da Interface do Usuário na inicialização (se disponível)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Menu tradicional (requer reinício)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Abrir o menu tradicional ao iniciar"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Aparência da tela inferior do 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Altera as configurações da aparência da tela inferior."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Ícone do aplicativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Alterar Ícone do Aplicativo."
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Menu rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Alterne a visibilidade dos itens do menu no \"Menu rápido\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Configurações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Alterna a visibilidade dos itens de menu nas \"Configurações\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Mostrar \"Carregar núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Mostra a opção \"Carregar núcleo\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Mostrar \"Carregar conteúdo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Mostra a opção \"Carregar conteúdo\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Mostrar \"Carregar disco\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Mostra a opção \"Carregar disco\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Mostrar \"Descarregar disco\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Mostra a opção \"Descarregar disco\" no menu principal."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Mostrar \"Ejetar disco\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Mostra a opção \"Ejetar disco\" no menu principal."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Mostrar \"Atualizações e downloads\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Mostre a opção \"Atualizações e downloads\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Mostrar \"Baixar núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Permite que você atualize núcleos (e arquivos de informação de núcleo) em \"Atualizações e downloads\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Mostrar \"Informações\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Mostra a opção \"Informações\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Mostrar \"Arquivo de configuração\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Mostre a opção \"Arquivo de configuração\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Mostrar \"Ajuda\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Mostra a opção \"Ajuda\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Mostrar \"Sair do RetroArch\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Mostre a opção \"Sair do RetroArch\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Mostrar \"Reiniciar o RetroArch\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Mostre a opção \"Reiniciar o RetroArch\" no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Mostrar \"Configurações\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Mostra o menu \"Configurações\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Defina a senha para ativar a aba \"Configurações\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Informar uma senha ao esconder a aba de configurações torna possível restaurá-la mais tarde a partir do menu, indo para o menu principal, selecionando \"Ative a aba Configurações\" e inserindo a senha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Mostrar \"Favoritos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Mostra o menu \"Favoritos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "Mostrar Favoritos Primeiro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "Mostra \"Favoritos\" antes de \"Histórico\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Mostrar \"Imagens\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Mostra o menu \"Imagens\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Mostrar \"Músicas\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Mostra o menu \"Músicas\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Mostrar \"Vídeos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Mostra o menu \"Vídeos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Mostrar \"Netplay\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Mostra o menu \"Netplay\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Mostrar \"Histórico\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Mostrar o menu de histórico recente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Mostrar \"Importar conteúdo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Mostrar a opção ‘Importar Conteúdo’ no Menu Principal ou em Listas de Reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Menu principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Menu das listas de reprodução"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Mostrar \"Playlists\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Exibe as playlists no Menu Principal. Ignorado no GLUI se as abas de playlist e a barra de navegação estiverem ativadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Exibir Abas de Playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Mostrar as abas da lista de reprodução. Não afeta o RGUI. A barra de navegação deve estar ativada no GLUI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Mostrar \"Explorar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Mostrar a opção explorador de conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Mostrar \"Núcleos sem conteúdo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Especifique o tipo de núcleo (se houver) a ser exibido no menu 'Núcleos Sem Conteúdo'. Quando definido como 'Personalizado', a visibilidade de núcleos individuais pode ser alternada pelo menu 'Gerenciar Núcleos'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Todos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Propósito-único"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Mostrar data e hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Mostra data e/ou hora atuais dentro do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Estilo da data e hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Altera o estilo da data atual ou como a hora é mostrada dentro do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Separador de data"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Especifica o caractere a ser usado como um separador entre os componentes dia/mês/ano quando a data atual é mostrada dentro do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Mostrar nível de bateria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Mostra o nível de bateria atual dentro do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Mostrar nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Mostra o nome do núcleo atual dentro do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Mostrar sub-etiquetas no menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Mostre as informações adicionais para os itens do menu."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Exibir tela inicial"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Mostra a tela inicial no menu. É automaticamente definido como falso após o programa iniciar pela primeira vez."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Mostrar \"Continuar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Mostra a opção que permite continuar o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Mostrar \"Reiniciar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Mostra a opção de reiniciar o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Mostrar \"Fechar conteúdo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Mostra a opção \"Fechar conteúdo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Mostrar submenu 'Save States'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Mostra as opções de ponto de save em um submenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Mostrar 'Salvar/Carregar Ponto de Save''"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Mostra as opções para salvar/carregar um ponto de save."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Exibir 'Controles de Replay'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Mostrar as opções para gravar/reproduzir arquivos de replay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Mostrar 'Desfazer Salvar/Carregar Ponto de Save'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Mostraras opções para desfazer salvar/carregar ponto de save."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Mostrar \"Configurações do núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Mostra a opção \"Configurações do núcleo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Mostrar \"Gravar opções em disco\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Mostra a opção \"Gravar opções em disco\" dentro do menu Opções -> Gerenciar opções do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Mostrar \"Controles\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Mostra a opção \"Controles\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Mostrar \"Captura de tela\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Mostre a opção \"Captura de tela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Mostrar 'Iniciar gravação'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Mostra a opção \"Iniciar gravação\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Mostrar 'Iniciar transmissão'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Mostre a opção 'Iniciar transmissão'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Mostrar 'Sobreposição na tela'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Mostre a opção 'Sobreposição na tela'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Mostrar 'Disposição de vídeo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Mostre a opção 'Disposição de vídeo'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Mostrar 'Latência'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Mostre a opção \"Latência\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Mostrar 'Retrocesso'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Mostra a opção de 'Retrocesso'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Mostrar 'Salvar personalizações de núcleo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Mostre a opção 'Salvar personalizações de núcleo' no menu 'Personalizações'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Exibir 'Substituições do Diretório de Conteúdo Salvo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Exibir a opção 'Substituições do Diretório de Conteúdo Salvo' no menu 'Substituições'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Mostrar 'Salvar personalizações de jogo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Mostre a opção 'Salvar personalizações de jogo' no menu 'Personalizações'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Mostrar 'Trapaças'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Mostra a opção \"Trapaças\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Mostrar \"Sombreadores\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Mostra a opção \"Sombreadores\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Mostrar \"Adicionar aos favoritos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Mostra a opção \"Adicionar aos favoritos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Mostrar \"Adicionar em uma lista\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Exibe a opção \"Adicionar em uma lista\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Mostrar \"Definir núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Mostra a opção \"Definir núcleo\" quando o conteúdo não estiver em execução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Mostrar \"Redefinir núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Mostra a opção \"Redefinir núcleo\" quando o conteúdo não estiver em execução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Mostrar 'Baixar miniaturas'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Mostra a opção \"Baixar miniaturas\" quando o conteúdo não estiver em execução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Mostrar \"Informações\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Mostra a opção \"Informações\"."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Mostrar 'Drivers'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Mostre as configurações dos \"Drivers\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Mostrar 'Vídeos'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Mostre as configurações do \"Vídeo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Mostrar 'Áudio'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Mostre as configurações do \"Áudio\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Mostrar 'Entrada'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Mostre as configurações da \"Entrada\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Mostrar \"Latência\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Mostre as configurações da \"Latência\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Mostrar 'Núcleo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Mostra as configurações do \"Núcleo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Mostrar \"Configuração\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Mostra os ajustes da \"Configuração\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Mostrar \"Salvamento\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Mostre as configurações do \"Salvamento\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Mostrar \"Registro de eventos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Mostre as configurações do \"Registro de eventos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Mostrar o \"Navegador de arquivos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Mostra as configurações do \"Navegador de arquivos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Mostrar \"Controle de quadros\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Mostre as configurações do \"Controle de quadros\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Mostrar \"Gravação\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Mostre as configurações da \"Gravação\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Mostrar \"Exibição na tela\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Mostre as configurações da \"Exibição na tela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Mostrar \"Interface de usuário\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Mostre as configurações da \"Interface de usuário\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Mostrar \"Serviço de IA\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Mostre as configurações do \"Serviço de IA\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Mostrar \"Acessibilidade\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Mostre as configurações da \"Acessibilidade\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Mostrar \"Gerenciamento de energia\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Mostre as configurações do \"Gerenciamento de energia\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Mostrar 'Conquistas'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Exibir configurações de 'Conquistas'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Mostrar \"Rede\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Mostre as configurações da \"Rede\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Mostrar \"Playlists\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Mostre as configurações da \"Listas de reprodução\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Mostrar \"Usuário\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Mostre as configurações do \"Usuário\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Mostrar 'Diretório'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Mostre as configurações do \"Diretório\"."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Mostrar \"Steam\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Mostra as configurações do \"Steam\"."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Fator de escala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Redimensiona o tamanho dos elementos da interface do usuário no menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Imagem de fundo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Seleciona uma imagem para definir como plano de fundo do menu. Imagens manuais e dinâmicas substituirão o 'Tema de Cores'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Opacidade do plano de fundo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Modifica a opacidade do plano de fundo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Opacidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Define a opacidade do fundo do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Use a cor preferida do tema do sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Use a paleta de cores do sistema operacional (se disponível). As personalizações do tema serão ignoradas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Miniatura principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Tipo de miniatura a ser exibida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Limite de redimensionamento de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Redimensiona automaticamente imagens em miniatura com altura e/ou largura menor do que o valor especificado. Tem um impacto moderado no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Fundos das Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Preenche o espaço não utilizado nas imagens em miniatura com um fundo sólido. Isso garante um tamanho de exibição uniforme para todas as imagens, melhorando a aparência do menu ao exibir itens com miniaturas de diferentes tamanhos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Animação de textos longos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Seleciona o método de rolagem horizontal usado para exibir longos textos do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Velocidade dos textos longos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Velocidade de animação ao rolar longas sequências de texto do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Suavizar rolagem dos textos longos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Use animação de rolagem suave ao exibir longos textos de menu. Tem um pequeno impacto no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Lembrar seleção ao alterar abas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Lembra a posição anterior do cursor ao mudar de aba. RGUI não tem abas, mas as listas de reproduções e configurações se comportam como abas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Sempre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Apenas para Listas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Apenas para o Menu Principal e as Configurações"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_STARTUP_PAGE,
   "Página de Inicialização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE,
   "Página inicial do menu na inicialização."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Formato de saída do serviço de IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Exibe a tradução como sobreposição de texto (Modo Imagem), reproduzir como Texto para Fala (Fala) ou usar um narrador do sistema como NVDA (Narrador)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL do serviço de IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Um endereço http:// url apontando para o serviço de tradução a ser usado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Serviço de IA habilitado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Habilita o serviço de IA para ser executado quando a tecla de atalho do serviço de IA for pressionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Pausar durante tradução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Pausa o núcleo enquanto a tela está sendo traduzida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Idioma de origem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "O idioma do qual o serviço será traduzido. Se definido como \"Padrão\", ele tentará detectar automaticamente. A configuração para um idioma específico tornará a tradução mais precisa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Idioma de destino"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "O idioma para o qual o serviço irá traduzir. Caso escolha a opção \"Padrão\" o idioma será o inglês."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Habilitar acessibilidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Habilita o narrador de acessibilidade para a navegação no menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Velocidade do narrador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Define a velocidade da voz do narrador."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Conquistas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Ganhe conquistas em jogos clássicos. Para mais informações, visite 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Conquistas do Modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Desativa trapaças, retrocesso, câmera lenta e o carregamento de save states. As conquistas do modo hardcore são marcadas de forma única para que você possa mostrar aos outros o que alcançou sem recursos de assistência do emulador. Alternar esta configuração durante a execução reiniciará o jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Tabelas de classificação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Envia periodicamente informações contextuais do jogo para o site RetroAchievements. Não tem efeito se o 'Modo Hardcore' estiver ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Insígnias de conquistas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Habilita ou desabilita a exibição de insígnia na Lista de Conquistas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Testar Conquistas Não Oficiais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Usa conquistas não oficiais e/ou recursos em versão beta para fins de teste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Som de desbloqueio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Reproduz um som quando uma conquista é desbloqueada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Capturar tela automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Captura a tela automaticamente ao completar uma conquista."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Modo jogar novamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Inicia a sessão com todas as conquistas ativas (inclusive as já desbloqueadas)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Aparência"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Altera a posição e os deslocamentos das notificações de conquistas na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Posição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Define o canto/borda da tela de onde as notificações de conquistas aparecerão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Superior Esquerdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Centro Superior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Superior Direito"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Inferior esquerdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Centro inferior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Inferior direito"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Preenchimento alinhado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Define se as notificações de conquistas devem alinhar-se com outros tipos de notificações na tela. Desative para definir manualmente os valores de espaçamento/posição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Preenchimento Horizontal Manual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Distância da borda esquerda/direita da tela, que pode compensar o overscan do display."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Preenchimento Vertical Manual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Distância da borda superior/inferior da tela, que pode compensar o overscan do display."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Visibilidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Altera quais mensagens e elementos na tela são exibidos. Não desativa funcionalidades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Resumo de inicialização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Exibe informações sobre o jogo carregado e o progresso atual do usuário.\n‘Todos os Jogos Identificados’ mostrará um resumo para jogos sem conquistas publicadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Todos os jogos identificados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Jogos com Consquistas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Desbloquear Notificações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Exibe uma notificação quando uma conquista é desbloqueada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Notificações de Domínio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Exibe uma notificação quando todas as conquistas de um jogo são desbloqueadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Indicadores de Desafio Ativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Exibe indicadores na tela enquanto certas conquistas podem ser obtidas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Indicador de Progresso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Exibe um indicador na tela quando há progresso em certas conquistas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Mensagens de Início do Ranking"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Exibe uma descrição do ranking quando ele fica ativo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Mensagens de Envio ao Ranking"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Exibe uma mensagem com o valor enviado quando uma tentativa no ranking é concluída."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Mensagens de Falha no Ranking"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Exibe uma mensagem quando uma tentativa no ranking falha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Rastreadores de Ranking"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Exibe rastreadores na tela com o valor atual dos rankings ativos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Mensagens de Login"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Exibe mensagens relacionadas ao login da conta RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Mensagens detalhadas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Exibe mensagens adicionais de diagnóstico e erro."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Anunciar publicamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Define a sala como pública. Caso desativada, os clientes deverão conectar-se manualmente em vez de usar a lista pública de salas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Usar servidor de retransmissão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Encaminha conexões de Netplay através de um servidor intermediário (man-in-the-middle). Útil se o anfitrião estiver atrás de um firewall ou tiver problemas de NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Localização do servidor de retransmissão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Escolha um servidor de retransmissão específico para usar. Locais geograficamente mais próximos tendem a ter menor latência."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Endereço do servidor personalizado de retransmissão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Insira o endereço do seu servidor de retransmissão personalizado. Formato suportado: endereço ou endereço|porta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "América do Norte (Costa Leste, EUA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Europa Ocidental"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "América do Sul (Sudeste, Brasil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Sudeste Asiático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Endereço do servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Endereço do servidor para se conectar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Porta TCP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Define a porta do endereço IP da sala. Pode ser uma porta TCP ou UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Limite de conexões"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Define a quantidade máxima de conexões simultâneas da sala. As salas cheias recusarão novas conexões automaticamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Limite de latência"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Define a latência (ping) máxima que a sala aceitará. Defina como \"0\" para desativar o limite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Senha do servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "A senha usada pelos clientes para se conectarem ao servidor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Senha do servidor (espectadores)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "A senha usada pelos clientes para se conectarem ao servidor como espectadores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Modo espectador da Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Inicia a Netplay no modo espectador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Define se o netplay será iniciado no modo espectador. Se definido como verdadeiro, o netplay começará no modo espectador. Sempre é possível mudar o modo depois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Desvanecer bate-papo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Desaparece as mensagens de bate-papo com um efeito de desvanecimento depois de um tempo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Cor do bate-papo (apelido)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Formato: #RRGGBB ou RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Cor do bate-papo (mensagem)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Formato: #RRGGBB ou RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Permitir pausar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Permite que os jogadores pausem o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Permitir clientes em modo escravo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Permite conexões em modo escravo. Os clientes no modo escravo exigem muito pouco poder de processamento em ambos os lados, mas sofrem significativamente com a latência da rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Não permitir clientes em modo não escravo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Não permitir conexões fora do modo escravo. Não recomendado, exceto em redes muito rápidas com máquinas muito lentas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Quadros de verificação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "A frequência (em quadros) da verificação de sincronia entre o servidor e os clientes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "Frequência em frames com a qual o netplay verificará se o host e o cliente estão sincronizados. Na maioria dos núcleos, esse valor não terá efeito visível e pode ser ignorado. Em núcleos não determinísticos, esse valor determina a frequência com que os participantes do netplay serão sincronizados. Em núcleos com bugs, definir qualquer valor diferente de zero pode causar sérios problemas de desempenho. Defina como zero para não realizar verificações. Esse valor é usado apenas no[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Latência em quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "O número de quadros de latência de entrada para a Netplay utilizar para mascarar a latência da rede. Reduz a oscilação e torna a Netplay menos intensiva para a CPU, ao custo de atraso perceptível na entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Número de frames da latência de entrada que o netplay usará para ocultar a latência da rede.\nDurante o netplay, essa opção atrasa a entrada local, fazendo com que o quadro em execução fique mais próximo dos quadros recebidos pela rede. Isso reduz variações (jitter) e torna o netplay menos exigente para a CPU, mas com o custo de um atraso perceptível na entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Faixa de latência em quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "O intervalo de quadros de latência de entrada que pode ser utilizado para mascarar a latência da rede. Reduz a oscilação e torna a Netplay menos intensiva para a CPU, ao custo de atraso imprevisível na entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Intervalo de quadros de latência de entrada que o netplay pode usar para ocultar a latência da rede.\nSe definido, o netplay ajustará dinamicamente o número de quadros de latência de entrada para equilibrar o uso da CPU, a latência da entrada e a latência da rede. Isso reduz variações (jitter) e torna o netplay menos exigente para a CPU, mas com o custo de um atraso imprevisível na entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Travessia de NAT da Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Ao hospedar uma partida, tente receber conexões da Internet pública usando UPnP ou tecnologias similares para escapar das redes locais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Compartilhamento de entrada digital"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Solicitar dispositivo %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Solicita jogar com o dispositivo de entrada indicado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Comandos de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Porta de comando de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "RetroPad de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Porta de base de RetroPad em rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "RetroPad em rede usuário %d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "Comandos stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Habilita a interface de comando stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Baixar miniaturas sob demanda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Faz o download automático de miniatura ausentes ao navegar pelas listas de reprodução. Tem um grande impacto no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Configurações do Atualizador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Acessar configurações do atualizador de núcleo"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "URL de núcleos do Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL para o diretório de atualização de núcleos no buildbot do Libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "URL de recursos do Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL para o diretório de atualizações de recursos no buildbot do Libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Extrair automaticamente o arquivo baixado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Após o download, extrair automaticamente os arquivos contidos nos arquivos comprimidos baixados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Mostrar núcleos experimentais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Inclui os núcleos 'experimentais' na opção Baixar núcleo. Normalmente, são apenas para fins de desenvolvimento ou testes e não são recomendados para o uso geral."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Fazer cópia dos núcleos ao atualizar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Cria automaticamente uma cópia de segurança de qualquer núcleo instalado ao executar uma atualização online. Permite uma fácil reversão para um núcleo funcional se uma atualização introduzir uma regressão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Tamanho do histórico da cópia de segurança"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Especifica quantas cópias de segurança geradas automaticamente devem ser mantidos para cada núcleo instalado. Ao atingir o limite, a criação de uma nova cópia por meio de uma atualização online excluirá a mais antiga. As cópias de segurança manuais não são afetadas por esta configuração."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Histórico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Habilita ou desabilita a lista de reprodução recente para jogos, imagens, música e vídeos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Tamanho do histórico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Limita o número de itens da lista de reprodução recente para jogos, imagens, música e vídeos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Tamanho dos favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Define o número máximo de entradas na lista de \"Favoritos\". Ao atingir o limite, novas entradas serão ignoradas até que as antigas sejam apagadas. Definir o valor para -1 permite entradas \"ilimitadas\".\nCUIDADO: A redução do valor apagará as entradas já existentes!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Permitir renomear itens"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Permite que os itens da playlist sejam renomeados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Permitir a remoção de itens"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Permite que os itens da lista de reprodução sejam removidos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Ordenar listas por ordem alfabética"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Ordena as listas de reprodução de conteúdo em ordem alfabética, excluindo as listas de reprodução \"Histórico\", \"Imagens\", \"Música\" e \"Vídeos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Salvar listas usando o formato antigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Salva as listas de reprodução usando o formato obsoleto de texto sem formatação. Quando esta opção estiver desativada, as listas de reprodução são criadas usando o formato JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Compactar listas de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Salva dados da lista de reprodução em um formato arquivado. Reduz o tamanho do arquivo e o tempo de carregamento às custas do (insignificante) aumento do uso da CPU. Pode ser usado com listas de reprodução de formato antigo ou novo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Mostrar núcleos associados nas listas de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Especifique quando marcar entradas da lista de reprodução com o núcleo atualmente associado (se houver).\nEssa configuração é ignorada quando as sub-etiquetas da lista de reprodução estão ativadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Mostrar sub-etiquetas da lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Mostra informações adicionais para cada item da lista de reprodução, como associação principal atual e tempo de jogo (se disponível). Tem um impacto de desempenho variável."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Mostrar ícones específicos do conteúdo no histórico e nos favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Mostra ícones específicos para cada item do histórico e da lista de reprodução favorita. Tem um impacto variável no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Núcleo:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Tempo de execução:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Último acesso:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_PLAY_COUNT,
   "Contagem de reprodução:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "segundo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "segundos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "minuto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "minutos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "hora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "horas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "dia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "dias"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "semana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "semanas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "mês"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "meses"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "ano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "anos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "atrás"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Mostrar índice dos itens da lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Mostra a quantidade de itens ao visualizar as listas de reprodução. O formato de exibição depende do driver do menu atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Tempo de execução de sub-etiquetas da lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Seleciona o tipo de registro de tempo de execução a ser exibido nas sub-etiquetas da lista de reprodução.\nO registro de tempo de execução correspondente deve ser ativado por meio do menu de opções \"Salvando\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Estilo da data e hora do \"Último acesso\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Define o estilo da data e hora exibidas nas informações do \"Último acesso\". As opções \"(AM/PM)\" terão um pequeno impacto de desempenho em algumas plataformas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Correspondência de arquivos difusos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Ao pesquisar nas listas de reprodução por itens associados a arquivos compactados, corresponder apenas ao nome do arquivo em vez de [nome do arquivo]+[conteúdo]. Habilite isso para evitar itens de histórico de conteúdo duplicados ao carregar arquivos compactados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Analisar sem correspondência do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Permite que o conteúdo seja analisado e adicionado a uma lista de reprodução sem um núcleo instalado que o suporte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Verificar CRC em Possíveis Duplicatas durante a Varredura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Às vezes, ISOs duplicam seriais, principalmente em títulos PSP/PSN. Confiar somente no serial pode fazer o scanner colocar o conteúdo no sistema errado. Isso adiciona uma verificação de CRC, que deixa a varredura bem mais lenta, mas pode ser mais precisa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Gerenciar playlists"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Executa tarefas de manutenção em listas de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Lista de reprodução portátil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Quando ativado e o diretório do \"Navegador de arquivos\" é escolhido, o valor atual do parâmetro \"Navegador de arquivos\" é salvo na lista de reprodução. Quando a lista é carregada em outro sistema onde a mesma opção está ativada, o valor do parâmetro \"Navegador de arquivos\" é comparado com o valor da lista de reprodução; se diferente, os caminhos dos itens da lista serão corrigidos automaticamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Usar Nomes de Arquivo para Correspondência de Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Quando ativado, encontrará miniaturas pelo nome do arquivo da entrada, em vez do seu rótulo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Permitir Todos os Tipos de Imagem Suportadas para Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "Quando ativado, miniaturas locais podem ser adicionadas em todos os tipos de imagem suportadas pelo RetroArch (como jpeg). Pode causar um pequeno impacto na performance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Gerenciar"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Núcleo padrão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Especifica o núcleo a ser usado ao iniciar o conteúdo por meio de um item da lista de reprodução que não tenha uma associação principal existente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Restaurar associações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Remova as associações existentes dos núcleos para todas as entradas da lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Modo de exibição das etiquetas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Altera a exibição das etiquetas de conteúdo nesta playlist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Modo de classificação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Determina como as entradas são classificadas nesta lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Limpar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Valida associações do núcleo e remove entradas inválidas e duplicadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Atualizar playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Adicione um novo conteúdo e remova as entradas inválidas repetindo a operação \"Análise manual\" usada pela última vez para criar ou para editar a lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Excluir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Exclui a playlist do sistema."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Privacidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Alterar configuração de privacidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Contas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Gerencia as contas configuradas atualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Nome de usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Define seu nome de usuário nas salas de Netplay e em outros lugares."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Idioma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Define o idioma da interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Localiza o menu e todas as mensagens na tela de acordo com o idioma selecionado aqui. Requer reinicialização para que as alterações tenham efeito.\nA completude da tradução é exibida ao lado de cada opção. Caso um idioma não esteja implementado para um item do menu, o padrão será o inglês."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Permitir câmera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Permite que os núcleos acessem a câmera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Presença rica do Discord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Permitir que o aplicativo do Discord mostre dados sobre o conteúdo jogado.\nSomente disponível com o cliente nativo do computador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Permitir localização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Permite que os núcleos acessem sua localização."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Ganhe conquistas em jogos clássicos. Para mais informações, visite 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Detalhes do login da sua conta RetroAchievements. Visite retroachievements.org e registre-se gratuitamente.\nApós o cadastro, insira o nome de usuário e a senha no RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK,
   "Jogos do Facebook"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Nome de usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Nome de usuário da sua conta do RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Senha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Senha da sua conta do RetroAchievements. Tamanho máximo: 255 caracteres."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Chave de transmissão do YouTube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Chave de transmissão do Twitch"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Chave de transmissão do Facebook Gaming"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Sistema/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS, ROMs de inicialização e outros arquivos específicos do sistema são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Os arquivos baixados são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Recursos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Os ativos do menu usados pelo RetroArch são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Planos de fundo dinâmicos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "As imagens de fundo usadas no menu são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Arte das caixas, captura de tela e miniaturas de tela são armazenadas neste diretório."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Diretório inicial"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Define o Diretório Inicial para o Navegador de Arquivos."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Arquivos de configuração"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "O arquivo de configuração padrão é armazenado neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Os núcleos Libretro são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Informações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Os arquivos de informação do aplicativo e núcleo são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Banco de dados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Os bancos de dados são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Arquivos de trapaça"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Os arquivos de trapaça são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Filtros de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Os arquivos de filtro baseados na CPU são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Filtros de áudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Os filtros DSP de áudio são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Sombreadores de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Os arquivos de filtro baseados na GPU são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Gravações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "As gravações são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Configuração de gravação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "As configurações de gravação são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Sobreposições"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "As sobreposições são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Sobreposição do Teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "As Sobreposições do Teclado são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Disposições de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "As sobreposições de vídeo são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Capturas de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "As capturas de tela são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Salvar os perfis de controle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Os perfis de controles usados para configurá-los automaticamente são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Remapeamentos de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Os remapeamentos de entrada são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "As listas de reprodução são armazenadas neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Lista de reprodução de favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Salve a lista de reprodução históricos neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Lista de reprodução de histórico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Salve a lista de reprodução de histórico neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Lista de reprodução de imagens"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Salve a lista de reprodução de imagens neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Lista de reprodução de música"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Salve a lista de reprodução de música neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Lista de reprodução de vídeos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Salve a lista de reprodução de vídeos neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Registros de tempo de execução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Os registro de tempo de execução são armazenados neste diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Arquivo de dados da memória do jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Armazena todos os arquivos de dados da memória do jogo neste diretório. Se não for definido, tentaremos salvar dentro do diretório de trabalho do arquivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Salve todos os arquivos de save (*.srm) neste diretório. Isso inclui arquivos relacionados como .rt, .psrm, etc. Isso será sobrescrito por opções explícitas de linha de comando."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Save states e replays são armazenados neste diretório. Se não definido, tentará salvá-los no diretório onde o conteúdo está localizado."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "O conteúdo arquivado será extraído temporariamente para este diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Registros de eventos do sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Os Registros de eventos do sistema são armazenados neste diretório."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Ativar presença rica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Compartilha o seu estado atual do RetroArch no Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Formato da presença rica do conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Decida quais informações relacionadas ao conteúdo serão compartilhadas."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Conteúdo (nome do sistema)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Conteúdo (nome do núcleo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Conteúdo (nome do sistema — nome do núcleo)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Adicionar ao mixer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Adiciona esta faixa de áudio a um compartimento de fluxo de áudio disponível.\nSe nenhum compartimento estiver disponível no momento, ele será ignorado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Adicionar ao mixer e reproduzir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Adiciona esta faixa de áudio a um compartimento de fluxo de áudio disponível e reproduz.\nSe nenhum compartimento estiver disponível no momento, ele será ignorado."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Anfitrião"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Conectar a um servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Permite digitar um endereço de servidor e conectar no modo cliente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Desconectar do Anfitrião do Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Desconecta de uma conexão ativa da Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Filtrar salas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Apenas salas que possam ser conectadas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Apenas núcleos instalados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Salas com senha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Atualizar lista de servidores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Procura por servidores na Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Atualizar rede local (LAN)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Procura por servidores na rede local."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Iniciar Anfitrião do Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Iniciar netplay em modo anfitrião (servidor)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Parar Anfitrião do Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Expulsar usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Expulsa um usuário da sua sala."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Banir usuário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Bane um usuário da sua sala."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Analisar diretório"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Analisa um diretório por arquivos compatíveis com o banco de dados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Analisar este diretório>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Selecione esta opção para escanear o diretório atual em busca de conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Analisar arquivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Analisa um arquivo compatível com o banco de dados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Analisar manualmente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Análise configurável com base nos nomes do conteúdo. Não requer que os conteúdos coincidam ao banco de dados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Escanear"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD,
   "Método de Busca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_METHOD,
   "Automático ou personalizado com opções detalhadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB,
   "Verificação de Banco de Dados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_USE_DB,
   "Rígido adicionará apenas itens que correspondem a uma entrada no banco de dados; Flexível adicionará também arquivos com a extensão correta, mas sem correspondência de CRC/serial; DAT Personalizado verifica contra um arquivo XML fornecido pelo usuário em vez dos bancos de dados; Nenhum ignora os bancos de dados e utiliza apenas as extensões de arquivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT,
   "Banco de Dados para Correspondência"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DB_SELECT,
   "A correspondência pode ser restrita a um banco de dados específico, ou ao primeiro banco de dados que corresponder, para acelerar a busca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST,
   "Playlist para Atualizar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_TARGET_PLAYLIST,
   "Os resultados são adicionados a esta playlist. No caso de Auto - Qualquer, múltiplas playlists de sistema podem ser atualizadas. Personalizado sem referência de banco de dados não vinculará as entradas a nenhum banco de dados na playlist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SINGLE_FILE,
   "Buscar Arquivo Único"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SINGLE_FILE,
   "Verifica apenas um arquivo em vez de um diretório. Selecione novamente o local do conteúdo após alterar esta entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_OMIT_DB_REF,
   "Pular Referências de Banco de Dados da Playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_OMIT_DB_REF,
   "No caso de um nome de playlist personalizado, use sempre o nome da playlist para a busca de miniaturas, mesmo que tenha ocorrido uma correspondência no banco de dados."
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Adicionar ao mixer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Adicionar ao mixer e reproduzir"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Diretório"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Define o diretório para analisar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Especifique um \"nome do sistema\" ao qual associar o conteúdo analisado. Usado para nomear o arquivo da lista de reprodução gerado e para identificar as miniaturas da lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Nome do sistema personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Manualmente, define um \"nome do sistema\" para o conteúdo analisado.\nOBSERVAÇÃO: requer \"Nome do sistema\" definido como \"<Personalizado>\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Núcleo padrão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Seleciona o núcleo que será usado para iniciar o conteúdo analisado por padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Extensões de arquivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Lista de tipos de arquivos a serem incluídos na análise, separados por espaços. Se vazio, inclui todos os tipos de arquivo ou, se um núcleo for especificado, todos os arquivos suportados pelo núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Analisar subdiretórios"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Analisa todos os subdiretórios do \"Diretório\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Analisar arquivos compactados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Ao habilitar esta opção, será pesquisado conteúdos válidos ou compatíveis dentro de arquivos compactados (.zip, .7z, etc.). Pode afetar o desempenho da análise."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arquivo .DAT de fliperama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Seleciona um arquivo DAT XML de Logiqx ou MAME para nomear automaticamente os conteúdos do arcade analisados (para MAME, FinalBurn Neo, etc.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Filtro DAT de arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Ao usar um arquivo DAT de arcade, o conteúdo só será adicionado à lista de reprodução se for encontrado um item correspondente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Substituir lista de reprodução existente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Quando habilitado, qualquer lista de reprodução existente será excluída antes da análise do conteúdo. Quando desabilitado, os itens da lista existentes serão preservados e apenas o conteúdo que estiver ausente dela será adicionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Validar entradas existentes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Quando ativado, as entradas existentes em qualquer lista de reprodução serão verificadas antes da leitura dos novos conteúdos. Serão removidas todas as entradas referentes ao conteúdo ausente e/ou arquivos com extensões inválidas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Iniciar análise"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Analisa o conteúdo selecionado."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Carregando lista…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Ano de lançamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Número de jogadores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Região"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Etiqueta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Pesquisar por nome…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Mostrar tudo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Filtro adicional"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Tudo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Adicionar filtro adicional"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u Itens"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Por desenvolvedor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Por distribuidora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Por ano de lançamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Por número de jogadores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Por gênero"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Por conquistas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Por categoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Por idioma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Por região"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Por exclusividade de console"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Por exclusividade de plataforma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Por suporte de vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Por nota"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Por mídia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Por controles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Por estilo artístico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Por jogabilidade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Por narrativa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Por ritmo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Por perspectiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Por ambientação"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Por visão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Por veículos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Por origem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Por franquia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Por etiqueta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Por nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Definir Alcance do Filtro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Visualização"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Salvar como visualização"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Deletar essa visualização"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Digite o nome da nova visualização"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Visualização já existe com o mesmo nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "A visualização foi salva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "A visualização foi excluída"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Executar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Inicia o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Renomear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Renomeia o título do item."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Remover"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Remove da playlist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Adicionar aos favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Adicionar o conteúdo aos \"Favoritos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Adicionar a uma playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Adiciona o conteúdo a uma playlist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Criar nova playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Cria uma nova lista de reprodução e adiciona o item atual nela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Definir núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Define o núcleo associado a este conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Redefinir núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Redefine o núcleo associado a este conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Mostra mais informações sobre o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Baixar miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Faz o download de imagens em miniatura da captura de tela, arte da caixa ou tela de título para o conteúdo atual. Atualiza quaisquer miniaturas existentes."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Núcleo atual"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Endereço"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Item: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Núcleo"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Tempo de jogo"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Último acesso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Banco de dados"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Continuar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Continua a execução do conteúdo atual e sai do menu rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Reinicia o conteúdo do início. O botão Start do RetroPad aciona um hard reset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Fechar conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Fecha o conteúdo atual. As alterações não salvas serão perdidas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Capturar tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Salva uma imagem da tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Compartimento de salvamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Altera o compartimento do jogo salvo selecionado atualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Ponto de Save"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Salva um ponto de save no slot atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVE_STATE,
   "Salva um ponto de save no slot atualmente selecionado. Nota: pontos de save normalmente não são portáteis e podem não funcionar em outras versões deste núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Carregar jogo salvo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Carrega um ponto de save do slot atualmente selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_STATE,
   "Carrega um ponto de save do slot atualmente selecionado. Nota: pode não funcionar se o ponto de save foi criado com outra versão do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Desfazer carregamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Se um jogo salvo for carregado, o conteúdo voltará ao estado anterior ao carregamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Desfazer salvamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Se um ponto de save for sobrescrito, ele voltará para o ponto de save anterior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Slot de Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Altera o compartimento do jogo salvo selecionado atualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Reproduzir Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Reproduzir arquivo de replay do slot selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Gravar replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Gravar arquivo de replay no slot selecionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Interromper Gravação/Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Interrompe a gravação/reprodução do replay atual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Adicionar aos favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Adiciona o conteúdo aos \"Favoritos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Iniciar gravação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Inicia a gravação de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Parar gravação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Para a gravação de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Iniciar transmissão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Inicia transmissão para o destino escolhido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Parar transmissão"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Termina a transmissão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Arquivos de salvamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Acessar opções do ponto de save."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Configurações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Altere as opções do conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Controles"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Altere os controles do conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Trapaças"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Configura os códigos de trapaça."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Controle de disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Gerenciamento de imagem de disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Sombreadores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Configura os sombreadores para realçar a aparência da imagem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Personalizações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Opções para substituir a configuração global."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Conquistas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Visualize conquistas e configurações relacionadas."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Gerenciar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Salve ou remova as personalizações que foram substituídas para o conteúdo atual."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Salvar configurações por conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Salva um arquivo de configurações exclusivo para o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Excluir configurações do conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Exclui o arquivo de configurações do conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Salvar opções do diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Salve as opções do núcleo que serão aplicadas a todo conteúdo carregado do mesmo diretório do arquivo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Remover Opções do Diretório de Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Exclui opções do núcleo que serão aplicadas a todo conteúdo carregado do mesmo diretório do arquivo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Arquivo atual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "Nome do arquivo de configuração do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Redefinir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Restaura todas as configurações do núcleo aos valores padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Gravar opções em disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Força as configurações atuais a serem salvas no arquivo de configurações atual. Garante que as configurações sejam preservada se uma falha no núcleo causar o fechamento da interface."
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Gerenciar arquivos de remapeamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Carrega, salva ou remove arquivos de remapeamento para o conteúdo atual."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Arquivo de Remapeamento Ativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "O arquivo de remapeamento atualmente em uso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Carregar arquivo de remapeamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Carrega e substitui os mapeamentos de entrada atuais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Salvar Arquivo de Remapeamento Como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Salva os mapeamentos de entrada atuais como um novo arquivo de remapeamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Salvar arquivo de remapeamento de núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Salva um arquivo de remapeamento que será aplicado para todo conteúdo carregado com este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Remover Arquivo de Remapeamento do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Exclui o arquivo de remapeamento que será aplicado para todo conteúdo carregado com este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Salvar remapeamento de controle para o diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Salva um arquivo de remapeamento que será aplicado para todo conteúdo carregado do mesmo diretório do arquivo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Remover Arquivo de Remapeamento do Diretório de Conteúdo do Jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Exclui o arquivo de remapeamento que será aplicado para todo conteúdo carregado do mesmo diretório do arquivo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Salva o arquivo de remapeamento do jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Salva um arquivo de remapeamento que será aplicado somente para o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Remover Arquivo de Remapeamento do Jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Exclui o arquivo de remapeamento que será aplicado somente para o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Restaurar mapeamento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Define todas as opções de remapeamento aos valores padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Atualizar arquivo de remapeamento de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Sobrescreve o arquivo de remapeamento ativo com as opções de remapeamento de entrada atuais."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Arquivo de remapeamento de controle"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Iniciar ou continuar a pesquisa de trapaças"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "Escanear a memória para criar novas trapaças."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Carregar arquivo de trapaça (substituir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Carrega um arquivo de trapaça e substitui as trapaças existentes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Carregar arquivo de trapaça (anexar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Carrega um arquivo de trapaça e anexa nas trapaças existentes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Recarregar Trapaças Específicas do Jogo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Salvar arquivo de trapaça como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Salvar as trapaças atuais como um arquivo de trapaça."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Adicionar nova trapaça no início"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Adicionar nova trapaça no final"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Excluir Todas as Trapaças"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Aplicar Trapaças Automaticamente Durante o Carregamento do Jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Aplica trapaças automaticamente ao carregar o jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplicar após alternar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplicar a trapaça imediatamente após alternância."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Aplicar alterações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "As alterações de trapaça terão efeito imediato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Trapaça"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Iniciar ou reiniciar a pesquisa de trapaças"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Pressione esquerda ou direita para alterar o tamanho do bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Pressione esquerda ou direita para alterar o valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Igual a %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Menos do que antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Menos ou igual a antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Maior que antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Maior ou igual a antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Igual a antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Diferente a antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Pressione esquerda ou direita para alterar o valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Igual a depois+%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Pesquisar valores de memória"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Pressione esquerda ou direita para alterar o valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Igual a antes-%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Adicionar as %u coincidências para sua lista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Excluir coincidência nº"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Criar coincidência de código nº"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Coincidir endereço: %08X Máscara: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Arquivo de trapaça (substituir)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Arquivo de trapaça (anexar)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Detalhes da trapaça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Índice"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Posição da trapaça na lista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Habilitar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Descrição"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Manipulador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Tamanho da memória de pesquisa"
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
   "Endereço da memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Examinar endereço: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Máscara do endereço da memória"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Bitmask o endereço quando o Tamanho da Busca da Memória para < 8-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Número de Iterações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "O número de vezes que a trapaça será aplicada. Use com as outras duas opções de iteração para afetar grandes áreas da memória."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Aumentar endereço em cada iteração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Após cada iteração, o \"Endereço da memória\" aumentará esse número vezes o \"Tamanho da memória de pesquisa\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Aumentar valor em cada iteração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Após cada \"Número de iterações\", o Valor será aumentado por esse valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Vibrar quando memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Porta de vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Força da vibração primária"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Duração da vibração primária (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Força da vibração secundária"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Duração da vibração secundária (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Código"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Adicionar nova trapaça depois desta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Adicionar nova trapaça antes desta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Copiar esta trapaça depois"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Copiar esta trapaça antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Remover esta trapaça"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Índice atual do disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Escolhe o disco atual a partir da lista de imagens disponíveis. A bandeja de disco virtual pode permanecer fechada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Ejetar disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Abre a bandeja de disco virtual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Inserir disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Fecha a bandeja de disco virtual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Carregar novo disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Seleciona um novo disco do sistema de arquivos e o adiciona à lista de índices.\nNOTA: Este é um recurso legado. Em vez disso, recomenda-se o uso de playlists M3U para títulos com vários discos."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Sombreadores de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Ativar Video shader pipeline."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Ver arquivos de shader para mudanças"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Aplicar automaticamente as mudanças feitas nos arquivos de shader no disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Monitora arquivos de shader para novas alterações. Após salvar mudanças em um shader no disco, ele será recompilado e aplicado automaticamente ao conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Lembrar do último diretório de shader usado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Abre o Navegador de Arquivos no último diretório usado ao carregar predefinições e passes de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Carregar predefinição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Carregar uma predefinição de shader. Será definido automaticamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Carrega um preset de shader diretamente. O menu de shaders é atualizado de acordo.\nO fator de escala exibido no menu é confiável apenas se o preset usar métodos de escala simples (ou seja, escala da fonte, mesmo fator de escala para X/Y)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Inserir Predefinição Antes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Insere a predefinição antes da predefinição atualmente carregada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Anexar Predefinição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Anexa a predefinição à predefinição atualmente carregada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_MANAGER,
   "Gerenciador de Predefinições"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER,
   "Salve ou remova predefinições de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_FILE_INFO,
   "Arquivo de Predefinição Ativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO,
   "A predefinição de shader atualmente em uso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Aplicar alterações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "As alterações das configurações de shader terão efeito imediato. Use isto se você alterou a quantidade de estágios de shader, filtros, escala FBO, etc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Após alterar configurações de shader, como quantidade de passes, filtragem, escala de FBO, use esta opção para aplicar as alterações.\nAlterar essas configurações de shader é uma operação relativamente custosa, então precisa ser feita explicitamente.\nQuando você aplica os shaders, as configurações são salvas em um arquivo temporário (retroarch.slangp/.cgp/.glslp) e carregadas. O arquivo permanece após o RetroArch ser encerrado e é salvo no Diretório de Shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Parâmetros de shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Modifica diretamente o shader atual. As alterações não serão salvas no arquivo de predefinição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Estágios de shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Aumenta ou diminui a quantidade de estágios do shader. Os shaders separados podem ser ligados a cada estágio e configurados sua escala e filtragem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "O RetroArch permite combinar diversos shaders com uma quantidade arbitrária de passes, usando filtros de hardware personalizados e fatores de escala.\nEsta opção especifica o número de passes de shader a serem usados. Se você definir como 0 e usar Aplicar Alterações de Shader, um shader em branco será usado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Caminho para o shader. Todos os shaders devem ser do mesmo tipo (ou seja, Cg, GLSL ou Slang). Defina o Diretório de Shaders para especificar onde o navegador começará a procurar pelos shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Filtro de hardware para este passe. Se \"Padrão\" for definido, o filtro será \"Linear\" ou \"Nearest\" dependendo da configuração de \"Filtragem Bilinear\" nas configurações de Vídeo."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Escala para este passe. O fator de escala se acumula, ou seja, 2x no primeiro passe e 2x no segundo passe resultará em uma escala total de 4x.\nSe houver um fator de escala para o último passe, o resultado será esticado para preencher a tela com o filtro padrão, dependendo da configuração de Filtragem Bilinear nas configurações de Vídeo.\nSe \"Padrão\" for definido, será usada uma escala de 1x ou esticada para tela cheia, dependendo se é ou não o último passe."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Predefinições simples"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Salva uma predefinição do Shader com um link para a predefinição original que já está carregada e inclui apenas as alterações que você fez no parâmetro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Salvar Predefinição Atual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Salva a predefinição do shader atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Salvar Predefinição Como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Salvar as definições de shader atuais como uma nova predefinição de Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Salvar predefinição global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Salva as configurações atuais do shader como a configuração global padrão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Salvar predefinição de núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Salva as configurações atuais do shader como padrão para este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Salvar predefinição do diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Salve as configurações atuais do shader como padrão para todos os arquivos no diretório de conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Salvar predefinição de jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Salvar as definições de shader atuais como a definição padrão para o conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Nenhuma predefinição automática de shader encontrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Remover predefinição global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Remove a predefinição global, usada por todos os conteúdos e núcleos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Remover predefinição de núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Remove a predefinição de núcleo, usada por todo o conteúdo executado com o núcleo atualmente carregado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Remover predefinição do diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Remove a predefinição do diretório de conteúdo, usada por todo o conteúdo dentro do diretório de trabalho atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Remover predefinição de jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Remove a predefinição de jogo, usada apenas para o jogo selecionado."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Não há parâmetros de sombreador"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Arquivo de predefinição ativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "O arquivo de predefinição atual está em uso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Carregar arquivo de predefinição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Carregar e substituir configuração atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Salvar arquivo de predefinição como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Salvar a configuração atual como um novo arquivo de substituição."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Salvar personalizações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Salva um arquivo de configuração que será aplicado a todo o conteúdo carregado por este núcleo. Terá prioridade sobre a configuração principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Remover Substituições do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Exclui o arquivo de substituição de configuração que será aplicado para todo o conteúdo carregado com este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Salvar personalizações do diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Salva um arquivo de configuração que será aplicado a todo o conteúdo carregado no mesmo diretório que o arquivo atual. Terá prioridade sobre a configuração principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Remover predefinição do diretório de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Exclui o arquivo de substituição de configuração que será aplicado para todo o conteúdo carregado a partir do mesmo diretório do arquivo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Salvar personalizações de jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Salva um arquivo de configuração que será aplicado apenas ao conteúdo atual. Terá prioridade sobre a configuração principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Remover Substituições do Jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Exclui o arquivo de substituição de configuração que será aplicado apenas para o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Descarregar Substituição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Redefinir todas as opções para os valores globais da configuração."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Nenhuma Conquista para Exibir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Cancelar Pausar as Conquistas do Modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Manter as conquistas do modo hardcore ativadas para a sessão atual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Cancelar Retomar as Conquistas do Modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Manter as conquistas do modo hardcore desativadas para a sessão atual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Retomada das Conquistas do Modo Hardcore Desativada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Você deve recarregar o núcleo para retomar as Conquistas do Modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Pausar conquistas no modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Pausar as conquistas do modo hardcore para a sessão atual. Esta ação ativará trapaças, retrocesso, câmera lenta e o carregamento de save states."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Retomar as Conquistas do Modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Retomar as conquistas do modo hardcore para a sessão atual. Esta ação desativará trapaças, retrocesso, câmera lenta e o carregamento de save states, além de reiniciar o jogo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "O servidor RetroAchievements está inacessível"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Uma ou mais conquistas alcançadas não chegaram ao servidor. As conquistas alcançadas serão enviadas novamente enquanto você deixar o aplicativo aberto."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "O servidor RetroAchievements está inacessível. Tentarei novamente até que a conexão seja bem-sucedida ou o aplicativo seja fechado."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Todas as solicitações pendentes foram sincronizadas com sucesso no servidor RetroAchievements."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Verificando o jogo"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Carregando dados do jogo"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Iniciando sessão"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Você não está logado"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Erro de rede"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Jogo desconhecido"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Conquistas não podem ser ativadas com este núcleo"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Hash do RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Item do banco de dados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Mostra as informações do banco de dados do conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Não há itens para mostrar"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Nenhum núcleo disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Não há configurações do núcleo disponíveis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Não há informação de núcleo disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Não há cópias de segurança disponíveis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Não há favoritos disponíveis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Não há histórico disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Nenhuma imagem disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Nenhuma música disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Nenhum vídeo disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Nenhuma informação disponível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Não há itens disponíveis na lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Nenhuma configuração encontrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Nenhum dispositivo Bluetooth foi encontrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Nenhuma rede encontrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Sem núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Procurar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Mudar as Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Seleção aleatória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Voltar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Diretório superior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Voltar para o diretório pai."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Diretório não encontrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Não há itens"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MORE,
   "…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Selecionar arquivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270º"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Compilador desconhecido"
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
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Compartilhamento de entrada analógica"
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
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Sem preferência"
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
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Modo imagem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Modo fala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Modo narrador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Histórico e favoritos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Todas as playlists"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Histórico e favoritos"
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
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Por núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Agregar"
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
   "não há fonte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Utilizar este diretório>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Selecione isto para definir como o diretório."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Diretório de conteúdo>"
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
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad com analógico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Desconhecido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Baixo + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Manter pressionado Start (2 segundos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Manter pressionado Select (2 segundos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Baixo + Select"
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
   "Não altera"
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
   "Igual ao valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "Diferente ao valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "Menor ao valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "Maior ao valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Aumenta o valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Diminui o valor da vibração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Todas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Desabilitado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Definir valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Aumentar por valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Diminuir por valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Executar a próxima trapaça caso o valor seja igual à memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Executar a próxima trapaça caso valor seja diferente da memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Executar a próxima trapaça caso valor seja menor que a memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Executar a próxima trapaça caso valor seja maior que a memória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emulador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1 bit, valor máx. = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2 bits, valor máx. = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4 bits, valor máx. = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8 bits, valor máx. = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16 bits, valor máx. = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32 bits, valor máx. = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Padrão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alfabético"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Mostrar tudo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Ocultar ()"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Ocultar []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Ocultar () e []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Mostrar região"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Mostrar índice do disco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Mostrar região e índice do disco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Padrão do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Arte da capa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Captura de tela"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Tela de título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Logo do Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Rápida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "LIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Sim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Não"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Verdadeiro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Falso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Habilitado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Desabilitado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "N/D"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Bloqueada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Desbloqueada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Não oficial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Não suportado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Desbloqueado recentemente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Quase lá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Desafios ativos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Apenas rastreadores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Apenas notificações"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Padrão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Conteúdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Diretório>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Personalizado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Não especificado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_AUTO,
   "Totalmente Automático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_STRICT,
   "Rígido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_LOOSE,
   "Flexível"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT,
   "DAT Personalizado (Rígido)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT_LOOSE,
   "DAT Personalizado (Flexível)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_NONE,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST_CUSTOM,
   "<Personalizado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Analógico esquerdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Analógico esquerdo (forçado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Analógico direito"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Analógico direito (forçado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG,
   "Analógicos Esquerdo + Direito"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG_FORCED,
   "Analógicos Esquerdo + Direito (Forçado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG,
   "Analógicos Twin-Stick"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG_FORCED,
   "Analógicos Twin-Stick (Forçado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Tecla %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Roda do mouse para cima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Roda do mouse para baixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Roda do mouse para esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Roda do mouse para direita"
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
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "AAAA-MM-DD HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "AAAA-MM-DD HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "AAAA-MM-DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "AAAA-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "MM-DD-AAAA HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "MM-DD-AAAA HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-DD-AAAA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "DD-MM-AAAA HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "DD-MM-AAAA HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "DD-MM-AAAA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "AAAA-MM-DD HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "AAAA-MM-DD HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-DD-AAAA HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-DD-AAAA HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "DD-MM-AAAA HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "DD-MM-AAAA HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Atrás"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Espessura do preenchimento de fundo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Aumentar a grossura do padrão xadrez de fundo do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Preenchimento da borda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Espessura do preenchimento da borda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Aumenta a grossura do padrão xadrez da borda do menu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Exibe a borda do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Usar disposição de largura total"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Redimensiona e posiciona as entradas do menu para aproveitar melhor o espaço disponível na tela. Desabilite isso para usar a disposição clássica de duas colunas de largura fixa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Filtro linear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Adiciona um leve desfoque para suavizar arestas dos pixels no menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Redimensionamento interno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Redimensiona a interface do menu antes de desenhar na tela. Quando usado com o \"Filtro linear de menu\" ativado, remove artefatos de escala (pixels ímpares) mantendo uma imagem nítida. Tem um impacto significativo no desempenho que aumenta com o nível de redimensionamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Seleciona a proporção do menu. As proporções widescreen aumentam a resolução horizontal da interface do menu. (pode exigir uma reinicialização se a opção \"Bloquear proporção de exibição do menu\" estiver desabilitada)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Bloquear proporção de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Garante que o menu seja sempre exibido com a proporção correta. Se desativado, o menu rápido será esticado para corresponder ao conteúdo atualmente carregado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Cor do Tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Define o tema de cores. \"Personalizado\" permite o uso dos arquivos de predefinição de temas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Predefinição de Tema Personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Selecione uma predefinição de tema do menu a partir do Navegador de Arquivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Transparência"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Ativa a exibição de fundo do conteúdo enquanto o Menu Rápido está ativo. Desativar a transparência pode alterar as cores do tema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Efeitos de sombra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Ativa as sombras projetadas para texto de menu, bordas e miniaturas. Tem um impacto modesto no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Animação de fundo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Ative o efeito de animação de partículas de fundo. Tem um impacto significativo no desempenho."
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
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Animação em segundo plano no protetor de tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Exibe o efeito de animação de partículas em segundo plano enquanto o protetor de tela estiver ativado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Mostrar miniaturas na lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Ativa a exibição de miniaturas redimensionadas embutidas ao visualizar listas de reprodução. Pode ser alternado com RetroPad Select. Quando desativado, as miniaturas ainda podem ser exibidas em tela cheia com RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Miniatura superior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Tipo de miniatura para exibir no canto superior direito das listas de reprodução. Pode ser alternado com o Analógico Direito para Cima/Esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Miniatura inferior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Tipo de miniatura para exibir no canto inferior direito das listas de reprodução. Pode ser alternado com o Analógico Direito para Baixo/Direita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Trocar miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Alterna as posições de exibição de \"Miniatura superior\" e \"Miniatura inferior\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Método de redução da escala de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Selecione o método de redimensionamento para que as miniaturas caibam na tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Atraso das miniaturas (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Aplica um intervalo de tempo entre a seleção de uma entrada da lista de reprodução e o carregamento de suas miniaturas associadas. Configurar isso para um valor de pelo menos 256 ms permite a rolagem rápida e sem atrasos até mesmo nos dispositivos mais lentos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Suporte a ASCII estendido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Ativa a exibição de caracteres ASCII não padrão. Necessário para compatibilidade com certos idiomas ocidentais não ingleses. Tem um impacto moderado no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Ícones de interruptor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Use ícones para simbolizar as opções ATIVADO/DESATIVADO ao invés de texto."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (lento)"
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
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (centralizado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (centralizado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (centralizado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (centralizado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (centralizado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "Automático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Ajustar à tela"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Dimensionar com valores inteiros"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Preenchimento de tela (esticada)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Vermelho clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Laranja clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Amarelo clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Verde clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Azul clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Violeta clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Cinza clássico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Vermelho antigo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Roxo escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Azul meia-noite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Dourado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Azul-elétrico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Verde maçã"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vermelho vulcânico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Lagoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
   "Drácula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Algodão-doce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackeando o kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solar escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solar claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango claro"
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
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX,
   "Fluxo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dinâmico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Cinza escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Cinza claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "DESLIGADA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Neve (leve)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Neve (forte)"
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
   "Estrelas"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Miniatura secundária"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Tipo de miniatura para exibir à esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Miniatura do Ícone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Tipo de miniatura do ícone da lista de reprodução a ser exibida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Plano de fundo dinâmico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Carrega dinamicamente um novo plano de fundo dependendo do contexto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Animação horizontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Ativa a animação horizontal para o menu. Isso terá um impacto no desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Destacar ícone de animação horizontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "A animação que é acionada ao rolar entre as abas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animação ao mover para cima/baixo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "A animação que é acionada ao mover para cima ou para baixo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animação ao fechar/abrir o menu principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "A animação que é acionada ao abrir um submenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Fator Alfa do Tema de Cor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON,
   "Ícone do Menu Atual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_CURRENT_MENU_ICON,
   "O ícone do menu atual pode ser ocultado, posicionado abaixo do menu horizontal ou no título do cabeçalho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NONE,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_TITLE,
   "Título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Seleciona uma fonte principal diferente para ser usada pelo menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Cor da fonte (vermelho)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Cor da fonte (verde)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Cor da fonte (azul)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Seleciona uma disposição diferente para a interface XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Tema dos ícones"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Seleciona um tema diferente de ícone para o RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Ícones de interruptor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Use ícones para simbolizar as opções ATIVADO/DESATIVADO ao invés de texto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Efeitos de sombra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Exibe sombras nos ícones, miniaturas e letras. Afeta ligeiramente o desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Sombreador do menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Seleciona um efeito de plano de fundo animado. Pode exigir mais processamento gráfico. Se o desempenho for insatisfatório, desligue este efeito ou reverta para um mais simples."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Cor do Tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Selecione um tema de cor de fundo diferente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Disposição Vertical das Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Exibe a miniatura esquerda sob a direita, no lado direito da tela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Fator de escala de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Reduz o tamanho da exibição de miniaturas dimensionando a largura máxima permitida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Desvanecimento vertical"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Mostrar cabeçalho"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Margem dos títulos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Deslocamento horizontal da margem do título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Ativar a Aba de Configurações"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Mostre a aba de configurações que contém as definições do programa."
   )

/* XMB: Settings Options */

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
   "Neve simples"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Neve"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Floco de neve"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Monocromático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Monocromático invertido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Sistemático"
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
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Verde maçã"
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
   "Azul manhã"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Violeta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Roxo escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Azul-elétrico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Dourado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Vermelho legado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Azul meia-noite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Imagem de fundo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Submarino"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vermelho vulcânico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Verde limão"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Amarelo Pikachu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Roxo Game Cube"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Vermelho Famicom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Quente flamejante"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Gelado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Cinza escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Cinza claro"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT,
   "Fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT,
   "Selecione uma fonte principal diferente para ser usada pelo menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE,
   "Escala da Fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "Define se o tamanho da fonte no menu deve ter sua própria escala e se ela deve ser aplicada globalmente ou com valores separados para cada parte do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "Valores separados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Fator de Escala da Fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Dimensiona o tamanho da fonte de forma linear em todo o menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Fator de Escala da Fonte do Título"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Dimensiona o tamanho da fonte do texto do título no cabeçalho do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Fator de Escala da Fonte da Barra Lateral Esquerda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Dimensiona o tamanho da fonte do texto na barra lateral esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Fator de Escala da Fonte dos Rótulos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Dimensiona o tamanho da fonte dos rótulos das opções do menu e das entradas da lista de reprodução. Também afeta o tamanho do texto nas caixas de ajuda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Fator de Escala da Fonte dos Subrótulos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Dimensiona o tamanho da fonte dos subrótulos das opções do menu e das entradas da lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "Fator de Escala da Fonte de Data e Hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "Dimensiona o tamanho da fonte do indicador de data e hora no canto superior direito do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Fator de Escala da Fonte do Rodapé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Dimensiona o tamanho da fonte do texto no rodapé do menu. Também afeta o tamanho do texto na barra lateral de miniaturas à direita."
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
   "Encurtar Nomes da Playlist (Requer reinício)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Remove os nomes do fabricantes das listas de reprodução. Por exemplo, \"Sony - PlayStation\" se torna \"PlayStation\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Ordenar Playlists Após Encurtamento do Nome (Requer reinício)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "As listas de reprodução serão reorganizadas em ordem alfabética após remover o componente do fabricante de seus nomes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Miniatura secundária"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Substitui o painel de metadados do conteúdo por outra miniatura."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Reduzir textos longos dos metadados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Ao habilitar esta opção, cada elemento dos metadados de um conteúdo a ser exibido na barra direita das listas de reprodução (núcleo associado, tempo de jogo...) vai ocupar apenas uma linha, cadeias que excedem a largura da barra se moverão automaticamente. Desabilitando, cada elemento dos metadados é apresentado estaticamente, estendendo as linhas conforme necessário."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Fator de escala de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Altera o tamanho da barra de miniaturas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "Fator de Preenchimento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "Ajustar o tamanho do preenchimento horizontal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON,
   "Ícone do Cabeçalho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_ICON,
   "O logo do cabeçalho pode ser oculto, dinâmico dependendo da navegação ou fixo no invasor clássico."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "Separador de Cabeçalho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "Largura alternativa para os separadores de cabeçalho e rodapé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_NONE,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_DYNAMIC,
   "Dinâmico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_FIXED,
   "Fixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "Nenhum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM,
   "Máximo"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Cor do Tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Define o tema de cores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Branco básico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Preto básico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackeando o kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Zona do crepúsculo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA,
   "Drácula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solar escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solar claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Cinza escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Cinza claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "Chuva Roxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SELENIUM,
   "Selênio"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Ícones"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Mostre os ícones à esquerda das entradas do menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Ícones de interruptor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Use ícones para simbolizar as opções ATIVADO/DESATIVADO ao invés de texto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Ícones da Playlist (Requer reinício)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Mostre os ícones específicos do sistema nas lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Otimizar a disposição no modo retrato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Ajusta automaticamente a disposição do menu para se adequar a tela com orientação em modo retrato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Mostrar barra de navegação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Exibe atalhos de navegação permanentes no menu da tela. Permite alternar rapidamente entre as categorias de menu. Recomendado para dispositivos com tela sensível ao toque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Girar automaticamente barra de navegação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Move automaticamente a barra de navegação no lado direito da tela na orientação em modo retrato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Cor do Tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Selecione um tema de cor de fundo diferente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Animação de Transição"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Habilita os efeitos de animação ao navegar entre diferentes opções de menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Miniaturas no modo retrato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Especifica como exibir miniaturas das listas de reprodução na orientação de retrato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Miniaturas no modo retrato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Especifica como exibir miniaturas das listas de reprodução na orientação em modo retrato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Mostrar miniatura secundária nas listas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Exibe uma miniatura secundária ao usar formatos de lista de miniaturas nas listas de reprodução. Observe que essa configuração só terá efeito se a tela tiver largura o suficiente para exibir duas miniaturas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Fundos das Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Preenche o espaço não utilizado nas imagens em miniatura com um fundo sólido. Isso garante um tamanho de exibição uniforme para todas as imagens, melhorando a aparência do menu ao exibir itens com miniaturas de diferentes tamanhos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Miniatura principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Miniatura principal relacionada aos itens nas listas de reprodução. Geralmente é o ícone do conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Miniatura secundária"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Miniatura auxiliar associada aos itens nas listas de reprodução. Ela será usada dependendo do modo de miniatura selecionado."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Azul"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Cinza azulado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Azul escuro"
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
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI (escuro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozone escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solar escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Cutie azul"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Cutie turquesa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Cutie verde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Cutie laranja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Cutie rosa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Cutie roxo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Cutie vermelho"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackeando o kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Cinza escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Cinza claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DRACULA,
   "Drácula"
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
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Lista (pequena)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Lista (média)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Dois ícones"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Lista (pequena)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Lista (média)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Lista (grande)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "DESLIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "LIGADO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Excluir imagens em miniatura"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Informações"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Arquivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Carregar núcleo…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Descarregar núcleo"
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
   "Docas fechadas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Parâmetros do sombreador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Configurações…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Lembrar posições da doca:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Lembrar geometria da janela:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Lembrar a última aba do explorador de conteúdo:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Tema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Padrão do sistema>"
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
   "Carregar núcleo personalizado..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Carregar núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Carregando núcleo…"
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
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Navegador de arquivos"
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
   "Explorador de conteúdo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Arte da capa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Captura de tela"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Tela de título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO,
   "Logotipo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Todas as playlists"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Informações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Me pergunte>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informações"
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
   "Erro de rede"
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
   "Associar núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Ocultar playlists"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Ocultar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Cor de destaque:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Escolher..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Selecionar cor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Selecionar tema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Tema personalizado"
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
   "Sugerir o primeiro núcleo a carregar:"
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
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Limpar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Progresso:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Nº máximo de entradas na lista de \"Todas as listas\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Nº máximo de entradas na grade de \"Todas as listas\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Mostrar arquivos e pastas ocultas:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nova lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Por favor, insira o novo nome da lista de reprodução:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Excluir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Renomear lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Tem certeza de que deseja excluir a lista de reprodução \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Informações"
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
   "Item da lista de reprodução"
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
   "Banco de dados:"
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
   "Adicionar item..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Adicionar arquivo(s)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Adicionar pasta…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Editar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Selecionar arquivos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Selecionar pasta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<múltiplos>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Erro ao atualizar o item da lista de reprodução."
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
   "Sombreador atual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Move para baixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Move para cima"
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
   "Remover estágios"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Aplicar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Adicionar estágio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Limpar todos os estágios"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Não há estágio de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Restaurar estágio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Restaurar todos os estágios"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Restaurar parâmetro"
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
   "Miniatura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Limite de cache de miniaturas:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Limite de tamanho para miniaturas de arrastar e soltar:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Baixar todas as miniaturas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Sistema completo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Esta lista de reprodução"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Miniaturas baixadas com sucesso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Obtidas: %1 Falhou: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Configurações do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Reiniciar tudo"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Configurações do Atualizador de Núcleos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Contas Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Fim da lista de contas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Contadores do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Nenhum disco selecionado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Contadores da interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Menu horizontal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Ocultar descritores de entrada do núcleo não vinculado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Exibir rótulos do descritor de entrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Sobreposição na tela"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Histórico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Seleciona o conteúdo do histórico recente da lista de reprodução."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Quando o conteúdo é carregado, as combinações de conteúdo e núcleo libretro são salvas no histórico.\nO histórico é salvo em um arquivo no mesmo diretório do arquivo de configuração do RetroArch. Se nenhum arquivo de configuração foi carregado na inicialização, o histórico não será salvo nem carregado, e não existirá no menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Multimídia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Subsistemas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Acessa as configurações do subsistema para o conteúdo atual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Conteúdo Atual: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Nenhum anfitrião do Netplay encontrado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Nenhum usuário da Netplay encontrado."
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
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Conectado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "On-line"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "Porta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Nome do dispositivo da porta %d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Nome de exibição: %s\nArquivo: %s\nVID/PID: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Configurações da trapaça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Iniciar ou continuar a pesquisa de trapaça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Reproduzir no Media Player"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "segundos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Iniciar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Inicia o núcleo sem conteúdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Núcleos sugeridos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Incapaz de ler o arquivo comprimido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Usuário"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Máximo de imagens na cadeia de troca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Informa ao driver de vídeo para utilizar explicitamente um modo de buffer específico."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Quantidade máxima de imagens na swapchain. Isso pode indicar ao driver de vídeo para usar um modo específico de buffer de vídeo.\nBuffer único - 1\nBuffer duplo - 2\nBuffer triplo - 3\nConfigurar o modo correto de buffering pode ter grande impacto na latência."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Swapchains Aguardáveis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Sincronia rígida entre CPU e GPU. Reduz a latência ao custo de desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Latência máxima de quadros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Informa ao driver de vídeo para utilizar explicitamente um modo de buffer específico."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modifica a predefinição de shader atualmente utilizada no menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Predefinição de shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Predefinição de shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Predefinição de shader"
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
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Apelido: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Procurando por conteúdo compatível..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Nenhum núcleo encontrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Nenhuma lista de reprodução encontrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Conteúdo compatível encontrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Falha ao localizar conteúdo correspondente pelo CRC ou nome do arquivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Condição"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Habilitar música em segundo plano do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Ajuda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Limpar"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Menu principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Jogando"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Jogando (pausado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Jogando"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Pausado"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay iniciará quando o conteúdo for carregado."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "O conteúdo deve ser carregado antes de iniciar o netplay."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Não foi possível encontrar um núcleo adequado ou arquivo de conteúdo, carregue manualmente."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Seu driver de gráficos não é compatível com o driver de vídeo atual no RetroArch, voltando para o driver %s. Por favor, reinicie o RetroArch para que as mudanças entrem em vigor."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Instalação do núcleo bem-sucedida"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Falha na instalação do núcleo"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Pressione para a direita cinco vezes para excluir todos as trapaças."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Volume global do mixer de áudio"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Análise da Netplay completa."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Desculpe, não implementado: núcleos que não exigem conteúdo não podem participar da Netplay."
   )
MSG_HASH(
   MSG_NATIVE,
   "Nativo"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Recebido um comando da Netplay desconhecido"
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
   "Atribuição de portas para Netplay concluída"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "A rede externa possui um endereço privado ou compartilhado. Considere usar um servidor de retransmissão."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Mapeamento das portas UPnP da Netplay falhou"
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
   "Aguardando pelocliente..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "A sua sala não está conectada na internet."
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
   "O jogador %.*s abandonou o jogo"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s se juntou como o jogador %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s juntou-se aos dispositivos de entrada %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYERS_INFO,
   "%d jogador(es)"
   )
MSG_HASH(
   MSG_NETPLAY_SPECTATORS_INFO,
   "%d jogador(es) (%d espectador(es))"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Uma tentativa de conexão da Netplay falhou porque o par não está executando o RetroArch ou está executando uma versão antiga do RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Um par do Netplay está usando uma versão antiga do RetroArch. Não é possível conectar."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "ATENÇÃO: Um par da Netplay está executando uma versão diferente do RetroArch. Se ocorrerem problemas, use a mesma versão."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Um par da Netplay está executando um núcleo diferente. Não é possível conectar."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "ATENÇÃO: Um par da Netplay está executando uma versão diferente do núcleo. Se ocorrerem problemas, use a mesma versão."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Este núcleo não tem suporte entre estas plataformas na Netplay"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Este núcleo não tem suporte entre plataformas diferentes na Netplay"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Digite a senha do servidor da Netplay:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Digite uma mensagem no bate-papo da Netplay:"
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
   "Um cliente da Netplay desconectou"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Desconectado da Netplay"
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
   "Par da Netplay \"%s\" pausou"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Seu apelido mudou para \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Usuário expulso: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Falha ao expulsar usuário: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Usuário banido: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Falha ao banir usuário: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Jogando"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "Assistindo"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Dispositivos"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Bate-papo compatível"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Lentidões Causadas"
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
   MSG_CAPABILITIES,
   "Capacidades"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Conectando ao anfitrião do Netplay"
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
   MSG_FETCHING_CORE_LIST,
   "Carregando lista de núcleos…"
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Falha ao obter a lista de núcleos!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Versão mais recente já instalada: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Atualizando o núcleo: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Baixando o núcleo: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Extraindo o núcleo: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Núcleo instalado: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Houve uma falha durante a instalação do núcleo: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Analisando núcleos…"
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Verificando o núcleo: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Atualização concluída"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Todos os núcleos compatíveis mudaram para as versões da Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Núcleos atualizados: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Núcleos ignorados: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "A atualização do núcleo foi desativada - o núcleo está bloqueado: "
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
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Limpando lista de reprodução: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Lista de reprodução limpa: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Falha na atualização - a lista de reprodução não contém nenhum registro de verificação válido: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Falha na atualização - conteúdo do diretório inválido/faltando: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Falha na atualização - nome do sistema inválido/faltando: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Falha na atualização - núcleo inválido: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Falha na atualização - arquivo DAT inválido/falta: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Falha na atualização - o arquivo DAT do arcade é grande demais (memória insuficiente): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Adicionado"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Falha ao adicionar favorito: lista de reprodução cheia"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Adicionado"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Lista de reprodução cheia"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Núcleo definido: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "A associação do núcleo dos itens da lista de reprodução foi redefinida."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Disco anexado"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Falha ao anexar disco"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Diretório do aplicativo"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Aplicando as alterações de trapaças."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Aplicando patch: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Aplicando sombreador"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Áudio desabilitado."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Áudio habilitado."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Houve um erro ao salvar o perfil do controle."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Perfil do controle salvo como \"%s\"."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Não foi possível inicializar o salvamento automático."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Salvar ponto de save automático em"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Trazendo a interface de comando na porta"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Não é possível inferir o novo caminho de configuração. Use a hora atual."
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
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "O host está executando um jogo diferente."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Seu ping está muito alto para este anfitrião."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Carregamento de conteúdo ignorado. A implementação irá carregar por conta própria."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "O núcleo não suporta save states."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATE_UNDO,
   "​O núcleo não suporta desfazer save state."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   "O núcleo não suporte Controle de Disco."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "O arquivo de configurações do núcleo foi criado com sucesso."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "O arquivo das configurações do núcleo foi removido com sucesso."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Todas as configurações do núcleo foram restauradas para os valores padrão."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "As configurações do núcleo foram salvas em:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Houve um erro ao salvar as configurações do núcleo em:"
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
   "Não foi possível ler o cabeçalho da gravação."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Não foi possível ler o estado da gravação."
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
   "A bandeja virtual do disco foi fechada."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "A bandeja virtual do disco foi ejetada."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Baixando"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "índice"
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
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Erro ao excluir o arquivo das configurações do núcleo."
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
   "Erro em salvar a predefinição de shader."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Diretório externo ao aplicativo"
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
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Falha em aceitar o espectador ingresso."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Falha em alocar memória para o conteúdo modificado..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Falha em aplicar o shader."
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
   "Falha aí carregar."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Falha ao carregar o conteúdo."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_FROM_PLAYLIST,
   "Falhou em carregar a lista de reprodução."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Falha ao carregar o arquivo da gravação."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Falha em carregar a sobreposição."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "A sobreposição de teclado não está definida."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Houve um erro ao carregar"
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
   "Falha ao receber apelido do anfitrião."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Falha ao receber tamanho do apelido do anfitrião."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Falha ao receber dados SRAM do anfitrião."
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
   MSG_FAILED_TO_LOAD_SRAM,
   "Houve uma falha ao carregar a SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Falha ao salvar ponto de save em"
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
   "Falha ao enviar apelido para o anfitrião."
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
   "Falha ao iniciar a gravação."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Houve um erro ao tentar iniciar a gravação."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Falha em obter uma captura de tela."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Falha em desfazer o carregamento de jogo salvo."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Falha ao desfazer ponto de save."
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
   "Ponto de save automático encontrado em"
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
   "Encontrada último compartimento de jogo salvo"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Último slot de replay encontrado"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Não é da gravação atual"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Não compatível com repetição"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE,
   "Não é possível carregar o ponto de save futuro durante a reprodução"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE,
   "Erro de linha do tempo durante a reprodução"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY,
   "Linha do tempo incorreta; substituindo gravação"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT,
   "Retroceder"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT_FAILED,
   "Falha ao Retroceder"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT,
   "Avançar"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT_FAILED,
   "Falha ao Avançar"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME,
   "Busca Concluída"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME_FAILED,
   "Falha na Busca"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Sombreador encontrado"
   )
MSG_HASH(
   MSG_FRAMES,
   "Quadros"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Configurações do núcleo personalizadas do jogo encontradas em"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Configurações do núcleo personalizadas do diretório encontradas em"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Índice de disco inválido obtido."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Capturar estado do mouse"
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
   "O núcleo Libretro é renderizado por hardware. Deve usar a gravação pós-shader também."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "A soma de verificação inflada não corresponde ao CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Entrada de trapaça"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Nome do arquivo de trapaça"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Nome de arquivo de predefinição"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "Nome do Arquivo de Substituição de Entrada"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Nome do Arquivo de Remapeamento de Entrada"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Renomear título"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Armazenamento interno"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Armazenamento removível"
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
   MSG_IN_MEGABYTES,
   "em megabytes"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "em gigabytes"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "foi compilado para outra versão do Libretro."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Interface do usuário para Libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Carregado do compartimento: %d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Estado carregado do compartimento: Auto."
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
   "Carregando jogo salvo"
   )
MSG_HASH(
   MSG_MEMORY,
   "Memória"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "O arquivo da gravação de replay não é um arquivo REPLAY válido."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "O formato da gravação de replay parece ter uma versão de serializador diferente. É muito provável que falhe."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "A reprodução da gravação de replay foi encerrada."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Parando a gravação."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Falha em inicializar a Netplay."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "O núcleo não tem suporte para Netplay."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Sem conteúdo, iniciando um núcleo vazio."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Nenhum ponto de save foi sobrescrito ainda."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Nenhum jogo salvo foi carregado até o momento."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Erro em salvar as personalizações."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "Erro ao remover substituições."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Personalizações salvas com sucesso."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "Substituições removidas com sucesso."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "Substituições descarregadas com sucesso."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "Nada para salvar. Substituições não salvas."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "Não está salvando. Substituições ativas."
   )
MSG_HASH(
   MSG_PAUSED,
   "Pausado."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Lendo a primeira faixa de dados..."
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
   "Redirecionando o arquivo de trapaça em"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Redirecionando arquivo dados da memória do jogo em"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Redirecionando ponto de save para"
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
   MSG_REMAP_FILE_RESET,
   "Todas as opções de remapeamento foram restauradas ao padrão."
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
   "Reiniciando"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Reiniciando a gravação devido ao reinício do driver."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Ponto de save antigo restaurado."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Sombreadores: restaurando predefinição padrão de sombreador em"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Revertendo diretório de arquivo de dados da memória do jogo em"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Revertendo diretório do ponto de save para"
   )
MSG_HASH(
   MSG_REWINDING,
   "Retrocedendo."
   )
MSG_HASH(
   MSG_REWIND_BUFFER_CAPACITY_INSUFFICIENT,
   "Capacidade do buffer insuficiente."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "O retrocesso está indisponível porque este núcleo não possui suporte a save state serializado."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Inicializando buffer de retrocesso com tamanho"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Falha ao inicializar o buffer de retrocesso. O retrocesso será desativado."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "A implementação usa áudio segmentado (threaded audio). Não é possível usar o retrocesso."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Fim do buffer de retrocesso atingido."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Configuração salva em"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Salvo no compartimento: %d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Estado salvo no compartimento: Auto."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Salvo com sucesso em"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Salvando tipo de RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Salvando"
   )
MSG_HASH(
   MSG_SCANNING,
   "Analisando"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Análise concluída"
   )
MSG_HASH(
   MSG_SCANNING_NO_DATABASE,
   "Escaneamento sem sucesso: nenhum banco de dados encontrado."
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
   "Sombreador"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Predefinição de shader salva com sucesso."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Câmera lenta."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Avanço rápido."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Retrocesso em câmera lenta."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Ignorando carregamento da SRAM."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM não será salva."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Bloqueando sobrescrita da SRAM"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Iniciando a reprodução da gravação."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Iniciando gravação em"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Tamanho do jogo salvo"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Compartimento de salvamento"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "Slot de Replay"
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
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Conquista Rara Desbloqueada"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Tentativa do ranking iniciou"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Tentativa do ranking falhou"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "Enviado %s para %s" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Classificação: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Melhor: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Alterar o tipo de miniatura"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Miniaturas em tela cheia"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Alternar metadados"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Nenhuma miniatura disponível"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Todas as possíveis tentativas de download de miniaturas já foram feitas para esta entrada da playlist."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Pressione novamente para sair..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
   "Pressione novamente para fechar o conteúdo..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_RESET,
   "Pressione novamente para reiniciar..."
   )
MSG_HASH(
   MSG_TO,
   "em"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Desfez o carregamento de jogo salvo."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Desfazendo ponto de save"
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
   "Comando desconhecido \"%s\" foi recebido.\n"
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
   "Falha no cálculo de tamanho da janela de exibição! Prosseguindo usando dados brutos. Isto poderá não funcionar corretamente..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Houve uma falha ao ejetar a bandeja virtual do disco."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Houve uma falha ao fechar a bandeja virtual do disco."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Carregando automaticamente ponto de save de"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "O carregamento automático do ponto de save de \"%s\" falhou."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "O carregamento automático do ponto de save de \"%s\" foi bem-sucesso."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s conectado na porta %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s desconectado da porta %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) não configurado"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) não configurado, usando padrão alternativo"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "A busca Bluetooth foi concluída."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Pareamento removido. Reinicie o RetroArch para conectar/parear novamente."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Análise de Wi-Fi completa."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Escaneando por dispositivos bluetooth..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Analisando redes sem fio..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Ativando o Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Desativando o Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Desconectando do Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Analisando por anfitriões da Netplay..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Preparando análise…"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Digite a senha"
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
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Digite a senha"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Senha correta"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Senha incorreta"
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
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Remapeamento de controle para o diretório de conteúdo carregado."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Arquivo de remapeamento principal carregado."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Opções de remapeamento de entrada salvadas em:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Falha ao salvar as opções de remapeamento de entrada em:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Antecipação de Execução ativada. Quadros de latência removidos: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Antecipação de Execução ativada com Segunda Instância. Quadros de latência removidos: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Antecipação de Execução desativada."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "A Antecipação de Execução foi desativada porque este núcleo não suporta save states."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Antecipação de Execução indisponível porque este núcleo carece de suporte determinístico a save state."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Falha ao salvar save state. Antecipação de Execução foi desativada."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Falha ao carregar save state. Antecipação de Execução foi desativada."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Falha ao criar segunda instância. Antecipação de Execução usará apenas uma instância."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Antecipação de Quadros ativada. Quadros de latência removidos: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Antecipação de Quadros desativada."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "A Antecipação de Quadros foi desativada porque este núcleo não suporta save states."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "Antecipação de Quadros indisponível porque este núcleo carece de suporte determinístico a save state."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Falha ao alocar memória para Antecipação de Quadros."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Falha ao salvar save state. Antecipação de Quadros foi desativada."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Falha ao carregar save state. Antecipação de Quadros foi desativada."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Análise concluída"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Busca por trapaças iniciada com sucesso."
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Falha ao iniciar a busca por trapaças."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "A busca ainda não foi inicializada/iniciada."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Número de coincidências = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "%u correspondências adicionadas."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Falha ao adicionar correspondências."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Código criado a partir da correspondência."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Falha ao criar o código."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Correspondência excluída."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Espaço insuficiente. O número máximo de trapaças simultâneas é 100."
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
   MSG_FAILED_TO_SET_DISK,
   "Falha ao definir disco."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Falha ao definir o último disco usado."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Falha ao conectar ao cliente."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Falha ao conectar ao host."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Host de netplay cheio."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Você foi banido deste host."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Falha ao receber o cabeçalho do host."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: Conectado como %s."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Você deve pausar ou desativar as Conquistas do Modo Hardcore para carregar save states."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_SAVEFILE_PREVENTED_BY_HARDCORE_MODE,
   "Você deve pausar ou desativar as Conquistas do Modo Hardcore para carregar saves srm."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Um save state foi carregado. Conquistas do Modo Hardcore desativadas para a sessão atual."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Uma trapaça foi ativada. Conquistas do Modo Hardcore desativadas para a sessão atual."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "As Conquistas do Modo Hardcore foram alteradas pelo anfitrião."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "O anfitrião do Netplay precisa ser atualizado. Conquistas do Modo Hardcore desativadas para a sessão atual."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "%s dominado"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "%s completo"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Conquistas do Modo hardcore ativadas; save state e retrocesso foram desativados."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Este jogo não possui conquistas."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "Todas as %d conquistas ativadas para esta sessão"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "Ativadas %d conquistas não oficiais"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Você desbloqueou %d de %d conquistas"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d não suportado"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_WARNING,
   "Conquistas não suportadas detectadas. Por favor, tente outro núcleo ou atualize o RetroArch."
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "Assistindo %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Hardcore pausado. Não é permitido definir manualmente o atraso de quadro de vídeo."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Modo Hardcore pausado. Intervalo de troca de vsync acima de 1 não é permitido."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Modo Hardcore pausado. Inserção de frame preto não é permitido."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Modo Hardcore pausado. Configuração não permitida: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Hardcore pausado. Você não pode ganhar conquistas de hardcore para %s usando %s"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: Jogo não pôde ser identificado."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "RetroAchievements: falha ao carregar jogo %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "RetroAchievements: falha ao alterar a mídia %s"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGIN_TOKEN_EXPIRED,
   "O login do RetroAchievements expirou. Por favor, digite sua senha novamente e reinicie o jogo."
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
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Alta"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Muito alta"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Aviso: Recursos ausentes, use o Atualizador Online se disponível."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Aviso: Fontes ausentes para o idioma selecionado, use o Atualizador Online se disponível."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Aviso: Idioma não compatível - usando Inglês."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Criando cópia do disco..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Unidade %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Carregue um núcleo primeiro."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Falha ao ler a unidade. Processo de criação de cópia do disco abortada."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Falha ao escrever para o disco. Processo de criação de cópia do disco abortada."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Nenhum disco inserido na unidade."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Predefinição de shader removida com sucesso."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Erro ao remover a predefinição do shader."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "O arquivo DAT de arcade selecionado é inválido."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "O arquivo DAT de arcade selecionado é muito grande (memória livre insuficiente)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Falha ao carregar o arquivo DAT do arcade (o formato é inválido?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Configuração de verificação manual inválida."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Nenhum conteúdo válido encontrado."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Analisando conteúdo: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Verificando as entradas atuais: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Analisando: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Limpando entradas M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Análise completa: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Analisando núcleo: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Já existe uma cópia de segurança do núcleo instalado: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Fazendo cópia de segurança do núcleo: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Removendo cópias de segurança obsoletas: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Cópia de segurança do núcleo completa: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Cópia de segurança do núcleo selecionado já está instalada: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Restaurando núcleo: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Restauração do núcleo completa: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "O arquivo do núcleo selecionado já está instalado: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Instalando núcleo: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Instalação do núcleo completa: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Arquivo de núcleo selecionado é inválido: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Falha ao fazer cópia de segurança do núcleo: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Falha na restauração do núcleo: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Falha na instalação do núcleo: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "A restauração do núcleo foi desativada - o núcleo está bloqueado: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "A instalação do núcleo foi desativada - o núcleo está bloqueado: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Houve uma falha ao fazer o bloqueio do núcleo: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Houve uma falha ao fazer o desbloqueio do núcleo: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Falha ao remover núcleo da lista de 'Núcleos sem Conteúdo': "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Falha ao adicionar núcleo à lista de 'Núcleos sem Conteúdo': "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "A exclusão do núcleo foi desativada - o núcleo está bloqueado: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Modo de vídeo não suportado"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Não é possível escrever no diretório de informações do núcleo: o cache de informação do núcleo será desativado"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Um jogo salvo foi encontrado em"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Carregando jogo salvo de"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "Falha ao entrar no GameMode"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "Falha ao entrar no GameMode: certifique-se de que o GameMode daemon está instalado e em execução"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Sincronização com a taxa de quadros exata do conteúdo ativada."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Sincronização com a taxa de quadros exata do conteúdo desativada."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "Taxa de atualização de vídeo alterada para %s Hz."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Atualizar Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Nome da interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Versão Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Reiniciar"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Joy-Con separados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Substituição da escala dos widgets gráficos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Aplica uma substituição manual da escala ao desenhar os widgets na tela. Aplica-se apenas quando \"Dimensionar widgets gráficos automaticamente\" está desativado. Pode ser usado para aumentar ou diminuir o tamanho das notificações, indicadores e controles decorados independentemente do próprio menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Resolução da tela"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Resolução da tela: padrão"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Resolução da tela: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Resolução da tela: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Aplicando: padrão"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Aplicando: %dx%d\nSTART para redefinir"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Aplicando: %dx%d - %s\nSTART para redefinir"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Redefinindo para: padrão"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Redefinindo para: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Redefinindo para: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Seleciona o Modo de Exibição (Reinício Necessário)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Desligar"
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
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Abra as configurações de permissão do Windows para ativar a capacidade broadFileSystemAccess."
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
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Filtro de tremulação de vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Gama de vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Habilitar filtro de suavização"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Procura por dispositivos bluetooth e os conecta."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Analisa redes sem fio e estabelece uma conexão."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Ativar o Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Conectar à rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Conectar à rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Desconectar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Reduzir tremulação de vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Definir largura de tela de VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Correção de overscan (superior)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Ajuste o corte do overscan da exibição reduzindo o tamanho da imagem pelo número especificado de linhas de varredura (tiradas da parte superior da tela). Pode introduzir artefatos de dimensionamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Correção de overscan (inferior)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Ajuste o corte do overscan da exibição reduzindo o tamanho da imagem pelo número especificado de linhas de varredura (tiradas da parte inferior da tela). Pode introduzir artefatos de dimensionamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Modo de desempenho sustentado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "Desempenho e potência da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Políticas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Modo de regulação"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Permite ajustar manualmente toda a CPU: regulador, frequências, etc. Recomendado apenas para usuários avançados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Desempenho (gerenciado)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "O modo padrão e recomendado. Providencia desempenho total em jogos enquanto economiza energia ao pausar o jogo ou navegar pelos menus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Gerenciamento personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Permite que você escolha quais reguladores serão usados dentro dos menus e durante um jogo. Recomenda-se usar Desempenho, Ondemand ou Schedutil durante o jogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Máximo desempenho"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Fornece sempre o máximo desempenho: as frequências mais altas para uma melhor experiência."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Potência mínima"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Use a frequência mais baixa disponível para economizar energia. Ideal para dispositivos usando baterias, mas reduzirá significativamente o desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Balanceado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Se adapta à carga de trabalho atual. Economiza energia e funciona bem com a maioria dos dispositivos e emuladores. Os jogos e núcleos mais exigentes podem sofrer quedas de desempenho em alguns dispositivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Frequência mínima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Frequência máxima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Frequência mínima do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Frequência máxima do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "Gerenciador de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Regulador de núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Regulador de menus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Modo de jogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Pode melhorar o desempenho, reduzir a latência e corrigir problemas de ruído no áudio. É necessário ter instalado https://github.com/FeralInteractive/gamemode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Ativar o GameMode do Linux pode melhorar a latência, corrigir problemas de estalos no áudio e maximizar o desempenho geral ao configurar automaticamente sua CPU e GPU para o melhor desempenho possível.\nO software GameMode precisa estar instalado para que isso funcione. Consulte https://github.com/FeralInteractive/gamemode para saber como instalar o GameMode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Utilizar modo PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Reiniciar o RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Sai e reinicia o RetroArch. Necessário para a ativação de determinadas configurações de menu (por exemplo, ao alterar o driver do menu)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Bloquear quadros"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Preferir toque frontal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Usar toque frontal em vez de toque traseiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Habilitar toque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Habilitar mapeamento de controle no teclado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Tipo de mapeamento para controle no teclado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Habilitar teclado pequeno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Tempo limite de bloqueio de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "O número de milissegundos a aguardar para obter uma amostra de entrada completa. Use-a se você tiver problemas com pressionamentos de botão simultâneos (somente Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Mostrar \"Reiniciar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Mostra a opção \"Reiniciar\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Mostrar \"Desligar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Mostra a opção \"Desligar\"."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Com senha"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "Internet (retransmitir)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (não conectável)"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Condição geral do armazenamento interno: Leitura e escrita"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Condição geral do armazenamento interno: Somente leitura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Brilho da tela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Aumenta ou diminui o brilho da tela."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Overclock da CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Faz um overclock na CPU do Switch."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "Habilitar bluetooth"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Habilita ou desabilita o bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Serviços"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Gerencia serviços ao nível de sistema operacional."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "Habilitar SAMBA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Compartilha pastas de rede através do protocolo SMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "Habilitar SSH"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Habilita ou desabilita o acesso remoto à linha de comando."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Ponto de acesso Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Habilita ou desabilita o ponto de acesso Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Fuso horário"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Selecione seu fuso horário para ajustar a data e a hora à sua localização."
   )
#ifdef HAVE_RETROFLAG
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "Retroflag Safe Shutdown"
#else
   "Retroflag Safe Shutdown (Reboot required)"
#endif
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "For use with compatible Retroflag case."
#else
   "For use with compatible Retroflag case. Reboot is required when changing."
#endif
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Exibe uma lista de fusos horários disponíveis. Após selecionar um fuso horário, a data e a hora são ajustadas para o fuso selecionado. Pressupõe que o relógio do sistema/hardware esteja configurado para UTC."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Opções do Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Gerenciar opções específicas do Nintendo Switch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "Overclock da CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "Ativar frequências de Overclock da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "Suporte a CEC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "Ativar a sincronização CEC com a TV ao conectar à dock"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Desativar ERTM do Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Desativa o ERTM do Bluetooth para corrigir o pareamento de alguns dispositivos"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Desconectando do ponto de acesso Wi-Fi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Desconectando do Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Conectando-se ao Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Sem SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Ponto de acesso Wi-Fi já iniciado"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "O ponto de acesso Wi-Fi não está em execução"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Iniciando ponto de acesso Wi-Fi com o SSID=%s e a senha=%s"
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
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Escala do mouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Ajusta a escala X/Y para a velocidade da light gun usando o Wiimote."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Escala de toque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Ajuste a escala x/y das coordenadas da tela tátil para acomodar o dimensionamento da tela de nível do SO."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "VMouse por Toque como Ponteiro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Ativar para repassar eventos de toque da tela sensível ao toque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "VMouse por Toque como Mouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Ativa a emulação de mouse virtual usando eventos de toque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "VMouse por Toque em Modo Touchpad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Ativa junto com o Mouse para usar a tela sensível ao toque como touchpad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "VMouse por Toque em Modo Trackball"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Ativa junto com o Mouse para utilizar a tela sensível ao toque como um trackball, adicionando inércia ao ponteiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "Gestos de Toque para o VMouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Ativa gestos na tela sensível ao toque, incluindo toques, arrastar com toque e deslizar com o dedo."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "Escala RGA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "Escala RGA e filtragem bicúbica. Pode quebrar os widgets."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Escala específica de contexto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Escala de contexto de hardware (se disponível)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "Ativar o clock do New3DS e o cache L2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "Ativa a velocidade do clock do New3DS (804MHz) e o cache L2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Tela inferior do 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Ativa a exibição de informações de status na tela inferior. Desative para aumentar a vida útil da bateria e melhorar o desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Modo de exibição do 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Seleciona entre os modos de exibição 3D e 2D. No modo \"3D\", os pixels são quadrados e um efeito de profundidade é aplicado ao visualizar o menu rápido. O modo \"2D\" oferece o melhor desempenho."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (efeito grade de pixel)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (alta resolução)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Toque na tela para ir\npara o menu do Retroarch"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "Recurso(s) não encontrado(s)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Nenhum\ndado"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Nenhuma\ncaptura de tela"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Continuar o jogo"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Criar\nPonto de Restauração"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Carregar\num ponto de\nrestauração"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Diretório dos recursos da tela inferior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Diretório dos recursos da tela inferior. O diretório precisa ter um \"bottom_menu.png\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Ativar fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Exibe a fonte do menu inferior. Ative para mostrar as descrições dos botões na parte inferior da tela. Isso exclui a data do jogo salvo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Vermelho da fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Ajusta a cor vermelha da fonte da tela inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Verde da fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Ajusta a cor verde da fonte da tela inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Azul da fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Ajusta a cor azul da fonte da tela inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Opacidade da fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Ajusta a opacidade da fonte da tela inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Tamanho da fonte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Ajusta o tamanho da fonte da tela inferior."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Verificação terminada.<br><br>\nPara que o conteúdo seja analisado corretamente, é necessário:\n<ul><li>ter um núcleo compatível já baixado</li>\n<li>ter os \"Arquivos de informação de núcleo\" atualizados via Atualizações e downloads</li>\n<li>ter o \"Banco de dados\" atualizada via Atualizações e downloads</li>\n<li>reiniciar o RetroArch caso alguma das situações acima tenha sido feita</li></ul>\nE finalmente, o conteúdo deve corresponder os bancos de dados existentes <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">aqui</a>. Se ainda não estiver funcionando, considere <a href=\"https://www.github.com/libretro/RetroArch/issues\">enviar um relatório de erro</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Toque do mouse está habilitado"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Toque do mouse está desabilitado"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "o microfone sdl2 requer um driver de áudio sdl2"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "Acessibilidade do RetroArch ativada. Carregar Núcleo no Menu Principal."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "parado."
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "Substituição do Jogador por IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION,
   "Permite que uma IA jogue no seu lugar"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_GAME_AI_OPTIONS,
   "IA do jogo"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "Substituir jogador2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "Substituir jogador 01"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "Substituir jogador2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "Substituir o jogador 02"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Mostrar depuração"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Mostrar depuração"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "Mostrar 'IA do Jogo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "Mostra a opção 'IA do Jogo'."
   )
#endif
#ifdef HAVE_SMBCLIENT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SETTINGS,
   "Configurações de Rede SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SETTINGS,
   "Define as configurações de compartilhamento de rede SMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_ENABLE,
   "Ativar Cliente SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_ENABLE,
   "Ativa o acesso a compartilhamento de rede via SMB. Recomenda-se fortemente o uso de Ethernet em vez de Wi-Fi para uma conexão mais estável. Nota: a alteração destas configurações requer a reinicialização do RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SERVER,
   "Servidor SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SERVER,
   "Endereço IP do Servidor ou nome do host (hostname)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SHARE,
   "Nome do Compartilhamento SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SHARE,
   "Nome do compartilhamento de rede para acessar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SUBDIR,
   "Subdiretório SMB (opcional)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SUBDIR,
   "Caminho do subdiretório no compartilhamento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_USERNAME,
   "​Nome de Usuário SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_USERNAME,
   "Nome de usuário para autenticação."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_PASSWORD,
   "Senha SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_PASSWORD,
   "Senha para autenticação."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_WORKGROUP,
   "Grupo de Trabalho SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_WORKGROUP,
   "Grupo de trabalho ou nome de domínio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_AUTH_MODE,
   "Modo de Autenticação SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_AUTH_MODE,
   "Selecione a autenticação usada no seu ambiente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_NUM_CONTEXTS,
   "Máximo de conexões SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_NUM_CONTEXTS,
   "Selecione o máximo de conexões usadas no seu ambiente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_TIMEOUT,
   "Tempo limite SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_TIMEOUT,
   "Selecione o tempo limite padrão em segundos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_BROWSE,
   "Navegador pelo Compartilhamento SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_BROWSE,
   "Navega pelos arquivos no compartilhamento SMB configurado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SMB_CLIENT,
   "Mostrar 'Cliente SMB'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SMB_CLIENT,
   "Mostra as configurações do 'Cliente SMB'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SMB_SHARE,
   "Compartilhamento SMB"
   )
#endif
