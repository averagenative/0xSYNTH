/*
 * 0xSYNTH CPLUG Plugin Configuration
 * Force-included by CPLUG wrapper sources via -include config.h
 */

#ifndef OXS_PLUGIN_CONFIG_H
#define OXS_PLUGIN_CONFIG_H

#define CPLUG_IS_INSTRUMENT    1
#define CPLUG_WANT_GUI         1  /* Stubs for now, real GUI in Phase 8b */
#define CPLUG_GUI_RESIZABLE    1
#define CPLUG_WANT_MIDI_INPUT  1
#define CPLUG_WANT_MIDI_OUTPUT 0

#define CPLUG_COMPANY_NAME   "0xSYNTH"
#define CPLUG_COMPANY_EMAIL  ""
#define CPLUG_PLUGIN_NAME    "0xSYNTH"
#define CPLUG_PLUGIN_URI     "https://github.com/averagenative/0xSYNTH"
#define CPLUG_PLUGIN_VERSION "0.1.0"

#define CPLUG_VST3_CATEGORIES "Instrument|Synth|Stereo"
#define CPLUG_VST3_TUID_COMPONENT  'OxSy', 'nthE', 'comp', 0x01
#define CPLUG_VST3_TUID_CONTROLLER 'OxSy', 'nthE', 'edit', 0x01

#define CPLUG_CLAP_ID          "com.0xsynth.plugin"
#define CPLUG_CLAP_DESCRIPTION "Multi-engine synthesizer (subtractive, FM, wavetable)"
#define CPLUG_CLAP_FEATURES    CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_STEREO, CLAP_PLUGIN_FEATURE_SYNTHESIZER

#endif /* OXS_PLUGIN_CONFIG_H */
