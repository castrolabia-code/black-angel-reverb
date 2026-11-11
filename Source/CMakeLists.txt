juce_add_plugin(BlackAngel
    COMPANY_NAME "KB Audio"
    COMPANY_WEBSITE "https://github.com/castrolabia-code/black-angel-reverb"
    PLUGIN_MANUFACTURER_CODE KbAu
    PLUGIN_CODE BkAn
    FORMATS VST3
    PRODUCT_NAME "KB REVERB - BLACK ANGEL"
    DESCRIPTION "Hybrid vocal reverb"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BlackAngel PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    Parameters/ParameterLayout.cpp)

target_compile_definitions(BlackAngel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BlackAngel
    PRIVATE
        BlackAngelDSP
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
