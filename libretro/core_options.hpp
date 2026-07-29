#pragma once

#include <libretro.h>

namespace ares_libretro_core_options {

struct Locale {
  unsigned language;
  const char* video;
  const char* emulation;
  const char* system;
  const char* renderer;
  const char* rendererInfo;
  const char* resolution;
  const char* resolutionInfo;
  const char* recompiler;
  const char* recompilerInfo;
  const char* expansionPak;
  const char* expansionPakInfo;
  const char* enabled;
  const char* disabled;
  const char* software;
};

static Locale locales[] = {
  {
    RETRO_LANGUAGE_PORTUGUESE_BRAZIL,
    "Vídeo",
    "Emulação",
    "Sistema",
    "Renderizador RDP",
    "Seleciona o renderizador do RDP do Nintendo 64. O paraLLEl-RDP usa Vulkan e aceleração por GPU; Software prioriza compatibilidade. Reinicie o conteúdo após alterar.",
    "Resolução interna",
    "Define a resolução do paraLLEl-RDP: SD (1×), HD (2×) ou UHD (4×). Resoluções maiores exigem mais da GPU. Reinicie o conteúdo após alterar.",
    "Recompilador da CPU/RSP",
    "Ativa a recompilação dinâmica da CPU e do RSP para melhorar o desempenho. Reinicie o conteúdo após alterar.",
    "Expansion Pak",
    "Conecta o Expansion Pak de 8 MiB. Desativado emula os 4 MiB padrão do Nintendo 64. Reinicie o conteúdo após alterar.",
    "Ativado",
    "Desativado",
    "Software",
  },
  {
    RETRO_LANGUAGE_PORTUGUESE_PORTUGAL,
    "Vídeo",
    "Emulação",
    "Sistema",
    "Renderizador RDP",
    "Seleciona o renderizador RDP da Nintendo 64. O paraLLEl-RDP utiliza Vulkan e aceleração por GPU; Software privilegia a compatibilidade. Reinicie o conteúdo após alterar.",
    "Resolução interna",
    "Define a resolução do paraLLEl-RDP: SD (1×), HD (2×) ou UHD (4×). Resoluções superiores exigem mais da GPU. Reinicie o conteúdo após alterar.",
    "Recompilador da CPU/RSP",
    "Ativa a recompilação dinâmica da CPU e do RSP para melhorar o desempenho. Reinicie o conteúdo após alterar.",
    "Expansion Pak",
    "Liga o Expansion Pak de 8 MiB. Desativado emula os 4 MiB padrão da Nintendo 64. Reinicie o conteúdo após alterar.",
    "Ativado",
    "Desativado",
    "Software",
  },
  {
    RETRO_LANGUAGE_SPANISH,
    "Vídeo",
    "Emulación",
    "Sistema",
    "Renderizador RDP",
    "Selecciona el renderizador RDP de Nintendo 64. paraLLEl-RDP usa Vulkan y aceleración por GPU; Software prioriza la compatibilidad. Reinicia el contenido después de cambiarlo.",
    "Resolución interna",
    "Define la resolución de paraLLEl-RDP: SD (1×), HD (2×) o UHD (4×). Las resoluciones más altas requieren más potencia de GPU. Reinicia el contenido después de cambiarla.",
    "Recompilador de CPU/RSP",
    "Activa la recompilación dinámica de CPU y RSP para mejorar el rendimiento. Reinicia el contenido después de cambiarla.",
    "Expansion Pak",
    "Conecta el Expansion Pak de 8 MiB. Desactivado emula los 4 MiB estándar de Nintendo 64. Reinicia el contenido después de cambiarlo.",
    "Activado",
    "Desactivado",
    "Software",
  },
  {
    RETRO_LANGUAGE_FRENCH,
    "Vidéo",
    "Émulation",
    "Système",
    "Moteur de rendu RDP",
    "Sélectionne le moteur de rendu RDP de la Nintendo 64. paraLLEl-RDP utilise Vulkan et l'accélération GPU ; Logiciel privilégie la compatibilité. Redémarrez le contenu après modification.",
    "Résolution interne",
    "Définit la résolution de paraLLEl-RDP : SD (1×), HD (2×) ou UHD (4×). Les résolutions élevées sollicitent davantage le GPU. Redémarrez le contenu après modification.",
    "Recompilateur CPU/RSP",
    "Active la recompilation dynamique du CPU et du RSP pour améliorer les performances. Redémarrez le contenu après modification.",
    "Pak d'extension",
    "Connecte le Pak d'extension de 8 Mio. Désactivé émule les 4 Mio standard de la Nintendo 64. Redémarrez le contenu après modification.",
    "Activé",
    "Désactivé",
    "Logiciel",
  },
  {
    RETRO_LANGUAGE_GERMAN,
    "Video",
    "Emulation",
    "System",
    "RDP-Renderer",
    "Wählt den RDP-Renderer des Nintendo 64. paraLLEl-RDP verwendet Vulkan und GPU-Beschleunigung; Software bevorzugt Kompatibilität. Inhalt nach einer Änderung neu starten.",
    "Interne Auflösung",
    "Legt die paraLLEl-RDP-Auflösung fest: SD (1×), HD (2×) oder UHD (4×). Höhere Auflösungen belasten die GPU stärker. Inhalt nach einer Änderung neu starten.",
    "CPU/RSP-Recompiler",
    "Aktiviert die dynamische Neukompilierung von CPU und RSP für bessere Leistung. Inhalt nach einer Änderung neu starten.",
    "Expansion Pak",
    "Verbindet das 8-MiB-Expansion-Pak. Deaktiviert emuliert die standardmäßigen 4 MiB des Nintendo 64. Inhalt nach einer Änderung neu starten.",
    "Aktiviert",
    "Deaktiviert",
    "Software",
  },
  {
    RETRO_LANGUAGE_ITALIAN,
    "Video",
    "Emulazione",
    "Sistema",
    "Renderer RDP",
    "Seleziona il renderer RDP del Nintendo 64. paraLLEl-RDP usa Vulkan e l'accelerazione GPU; Software privilegia la compatibilità. Riavvia il contenuto dopo la modifica.",
    "Risoluzione interna",
    "Imposta la risoluzione di paraLLEl-RDP: SD (1×), HD (2×) o UHD (4×). Le risoluzioni superiori richiedono più potenza GPU. Riavvia il contenuto dopo la modifica.",
    "Ricompilatore CPU/RSP",
    "Attiva la ricompilazione dinamica di CPU e RSP per migliorare le prestazioni. Riavvia il contenuto dopo la modifica.",
    "Expansion Pak",
    "Collega l'Expansion Pak da 8 MiB. Disabilitato emula i 4 MiB standard del Nintendo 64. Riavvia il contenuto dopo la modifica.",
    "Abilitato",
    "Disabilitato",
    "Software",
  },
  {
    RETRO_LANGUAGE_JAPANESE,
    "ビデオ",
    "エミュレーション",
    "システム",
    "RDPレンダラー",
    "Nintendo 64のRDPレンダラーを選択します。paraLLEl-RDPはVulkanとGPUアクセラレーションを使用し、ソフトウェアは互換性を優先します。変更後にコンテンツを再起動してください。",
    "内部解像度",
    "paraLLEl-RDPの解像度をSD（1×）、HD（2×）、UHD（4×）から選択します。高い解像度ほどGPU負荷が増えます。変更後にコンテンツを再起動してください。",
    "CPU/RSPリコンパイラー",
    "CPUとRSPの動的再コンパイルを有効にして性能を向上させます。変更後にコンテンツを再起動してください。",
    "拡張パック",
    "8 MiBの拡張パックを接続します。無効にするとNintendo 64標準の4 MiBをエミュレートします。変更後にコンテンツを再起動してください。",
    "有効",
    "無効",
    "ソフトウェア",
  },
  {
    RETRO_LANGUAGE_CHINESE_SIMPLIFIED,
    "视频",
    "模拟",
    "系统",
    "RDP 渲染器",
    "选择 Nintendo 64 的 RDP 渲染器。paraLLEl-RDP 使用 Vulkan 和 GPU 加速；软件渲染优先保证兼容性。更改后请重新启动内容。",
    "内部分辨率",
    "设置 paraLLEl-RDP 分辨率：SD（1×）、HD（2×）或 UHD（4×）。更高分辨率需要更多 GPU 性能。更改后请重新启动内容。",
    "CPU/RSP 重编译器",
    "启用 CPU 和 RSP 动态重编译以提高性能。更改后请重新启动内容。",
    "内存扩展包",
    "连接 8 MiB 内存扩展包。禁用时模拟 Nintendo 64 标准的 4 MiB 内存。更改后请重新启动内容。",
    "启用",
    "禁用",
    "软件",
  },
};

static retro_core_option_v2_category englishCategories[] = {
  {
    "video",
    "Video",
    "Configure the RDP renderer and internal rendering resolution.",
  },
  {
    "emulation",
    "Emulation",
    "Configure CPU and RSP emulation.",
  },
  {
    "system",
    "System",
    "Configure Nintendo 64 hardware.",
  },
  {},
};

static retro_core_option_v2_definition englishDefinitions[] = {
  {
    "ares_n64_renderer",
    "RDP Renderer",
    "Renderer",
    "Selects the Nintendo 64 RDP renderer. paraLLEl-RDP uses Vulkan and GPU acceleration; Software prioritizes compatibility. Restart content after changing.",
    nullptr,
    "video",
    {
      {"paraLLEl-RDP (Vulkan)", "paraLLEl-RDP (Vulkan)"},
      {"Software", "Software"},
      {nullptr, nullptr},
    },
    "paraLLEl-RDP (Vulkan)",
  },
  {
    "ares_n64_quality",
    "Internal Resolution",
    "Internal Resolution",
    "Sets the paraLLEl-RDP resolution: SD (1×), HD (2×), or UHD (4×). Higher resolutions require more GPU performance. Restart content after changing.",
    nullptr,
    "video",
    {
      {"SD", "SD (1×)"},
      {"HD", "HD (2×)"},
      {"UHD", "UHD (4×)"},
      {nullptr, nullptr},
    },
    "SD",
  },
  {
    "ares_n64_recompiler",
    "CPU/RSP Recompiler",
    "Recompiler",
    "Enables dynamic CPU and RSP recompilation for better performance. Restart content after changing.",
    nullptr,
    "emulation",
    {
      {"enabled", "Enabled"},
      {"disabled", "Disabled"},
      {nullptr, nullptr},
    },
    "enabled",
  },
  {
    "ares_n64_expansion_pak",
    "Expansion Pak",
    "Expansion Pak",
    "Connects the 8 MiB Expansion Pak. Disabled emulates the Nintendo 64's standard 4 MiB. Restart content after changing.",
    nullptr,
    "system",
    {
      {"enabled", "Enabled"},
      {"disabled", "Disabled"},
      {nullptr, nullptr},
    },
    "enabled",
  },
  {},
};

static retro_core_options_v2 englishOptions = {
  englishCategories,
  englishDefinitions,
};

static retro_core_option_v2_category localizedCategories[4] = {};
static retro_core_option_v2_definition localizedDefinitions[5] = {};
static retro_core_options_v2 localizedOptions = {
  localizedCategories,
  localizedDefinitions,
};

inline auto findLocale(unsigned language) -> const Locale* {
  for(auto& locale : locales) {
    if(locale.language == language) return &locale;
  }
  return nullptr;
}

inline auto buildLocalizedOptions(const Locale& locale) -> void {
  localizedCategories[0] = {"video", locale.video, nullptr};
  localizedCategories[1] = {"emulation", locale.emulation, nullptr};
  localizedCategories[2] = {"system", locale.system, nullptr};
  localizedCategories[3] = {};

  localizedDefinitions[0] = {
    "ares_n64_renderer",
    locale.renderer,
    locale.renderer,
    locale.rendererInfo,
    locale.rendererInfo,
    "video",
    {
      {"paraLLEl-RDP (Vulkan)", "paraLLEl-RDP (Vulkan)"},
      {"Software", locale.software},
      {nullptr, nullptr},
    },
    "paraLLEl-RDP (Vulkan)",
  };
  localizedDefinitions[1] = {
    "ares_n64_quality",
    locale.resolution,
    locale.resolution,
    locale.resolutionInfo,
    locale.resolutionInfo,
    "video",
    {
      {"SD", "SD (1×)"},
      {"HD", "HD (2×)"},
      {"UHD", "UHD (4×)"},
      {nullptr, nullptr},
    },
    "SD",
  };
  localizedDefinitions[2] = {
    "ares_n64_recompiler",
    locale.recompiler,
    locale.recompiler,
    locale.recompilerInfo,
    locale.recompilerInfo,
    "emulation",
    {
      {"enabled", locale.enabled},
      {"disabled", locale.disabled},
      {nullptr, nullptr},
    },
    "enabled",
  };
  localizedDefinitions[3] = {
    "ares_n64_expansion_pak",
    locale.expansionPak,
    locale.expansionPak,
    locale.expansionPakInfo,
    locale.expansionPakInfo,
    "system",
    {
      {"enabled", locale.enabled},
      {"disabled", locale.disabled},
      {nullptr, nullptr},
    },
    "enabled",
  };
  localizedDefinitions[4] = {};
}

inline auto registerOptions(retro_environment_t callback) -> void {
  if(!callback) return;

  unsigned version = 0;
  if(
    callback(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) &&
    version >= 2
  ) {
    unsigned language = RETRO_LANGUAGE_ENGLISH;
    callback(RETRO_ENVIRONMENT_GET_LANGUAGE, &language);

    const Locale* locale = findLocale(language);
    if(locale) buildLocalizedOptions(*locale);

    retro_core_options_v2_intl options = {
      &englishOptions,
      locale ? &localizedOptions : nullptr,
    };
    callback(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &options);
    return;
  }

  static const retro_variable variables[] = {
    {
      "ares_n64_renderer",
      "RDP renderer; paraLLEl-RDP (Vulkan)|Software"
    },
    {
      "ares_n64_quality",
      "Internal resolution; SD|HD|UHD"
    },
    {
      "ares_n64_recompiler",
      "CPU/RSP recompiler; enabled|disabled"
    },
    {
      "ares_n64_expansion_pak",
      "Expansion Pak; enabled|disabled"
    },
    {nullptr, nullptr},
  };
  callback(
    RETRO_ENVIRONMENT_SET_VARIABLES,
    const_cast<retro_variable*>(variables)
  );
}

} // namespace ares_libretro_core_options
