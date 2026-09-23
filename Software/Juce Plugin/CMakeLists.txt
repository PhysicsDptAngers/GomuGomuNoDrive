cmake_minimum_required(VERSION 3.22)

project(GomuGomuNoDrive VERSION 0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Inclusion de JUCE (attendu dans ./JUCE)
add_subdirectory(JUCE)

# 2. Téléchargement automatique des extensions CLAP
include(FetchContent)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap-juce-extensions)

# Détection automatique de l'ensemble des fichiers IR (GPL)
file(GLOB_RECURSE IR_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IR/BestPlugins_Amps/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IR/BestPlugins_Bands/*.wav"
)

# Génération des ressources binaires embarquées
juce_add_binary_data(GomuGomuIRs
    SOURCES
        ${IR_FILES}
)

# Déclaration de la cible et des formats audio natifs JUCE
juce_add_plugin(GomuGomuNoDrive
    PLUGIN_MANUFACTURER_CODE Juce   
    PLUGIN_CODE Dem0                            
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COMPANY_NAME "University of Angers"
    BUNDLE_ID "com.universityofangers.gomugomunodrive"
    LV2URI "urn:matth:gomugomunodrive"
    FORMATS VST3 Standalone LV2
    PRODUCT_NAME "GomuGomuNoDrive"
    PLUGIN_NAME "ゴムゴムのDrive"
)

# Activation du format CLAP via l'extension
clap_juce_extensions_plugin(
    TARGET GomuGomuNoDrive
    CLAP_ID "com.universityofangers.gomugomunodrive"
    CLAP_FEATURES "audio-effect" "distortion"
)

# Ajoute CLAP à la cible globale de build si elle existe
if(TARGET GomuGomuNoDrive_All)
    add_dependencies(GomuGomuNoDrive_All GomuGomuNoDrive_CLAP)
endif()

# Fichiers sources du projet
target_sources(GomuGomuNoDrive
    PRIVATE
        PluginEditor.cpp
        PluginProcessor.cpp
)

# Définitions de compilation
target_compile_definitions(GomuGomuNoDrive
    PUBLIC
        JUCE_WEB_BROWSER=0  
        JUCE_USE_CURL=0     
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Optimisations DSP temps réel (inlining mathématique et SIMD)
target_compile_options(GomuGomuNoDrive
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -fno-math-errno
            -fomit-frame-pointer
            -msse4.2
        >
)

# Édition de liens (LTO actif via juce_recommended_lto_flags)
target_link_libraries(GomuGomuNoDrive
    PRIVATE
        GomuGomuIRs
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
