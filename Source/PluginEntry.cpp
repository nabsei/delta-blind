#include "DeltaMatchProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeltaMatchProcessor();
}
