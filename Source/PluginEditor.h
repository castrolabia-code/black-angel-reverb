// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

/** Stage 2 editor: JUCE's generic parameter editor, so every control can be
    heard and automated. The BLACK ANGEL interface replaces it in Stage 5. */
class BlackAngelEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit BlackAngelEditor (BlackAngelProcessor& p) : juce::GenericAudioProcessorEditor (p)
    {
        setSize (560, 640);
        setResizable (true, true);
    }
};
