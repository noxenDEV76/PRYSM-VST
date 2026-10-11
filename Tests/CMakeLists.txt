juce_add_console_app(prysm_tests PRODUCT_NAME "prysm_tests")
target_sources(prysm_tests PRIVATE EngineTests.cpp)
target_link_libraries(prysm_tests PRIVATE prysm_core)
target_compile_definitions(prysm_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

juce_add_console_app(prysm_vst3_host PRODUCT_NAME "prysm_vst3_host")
target_sources(prysm_vst3_host PRIVATE Vst3HostTest.cpp)
target_compile_definitions(prysm_vst3_host PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(prysm_vst3_host PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_recommended_config_flags)

add_test(NAME engine_and_processor COMMAND prysm_tests)

# VST3 host test: loads the *built* plug-in binary exactly like a DAW would.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME make_test_sample
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/make_test_sample.py ${CMAKE_BINARY_DIR}/test_sample.wav)
    set_tests_properties(make_test_sample PROPERTIES FIXTURES_SETUP prysm_sample)
    add_test(NAME vst3_host
             COMMAND prysm_vst3_host "${CMAKE_BINARY_DIR}/PRYSM_artefacts/$<CONFIG>/VST3/PRYSM.vst3" ${CMAKE_BINARY_DIR}/test_sample.wav)
    set_tests_properties(vst3_host PROPERTIES FIXTURES_REQUIRED prysm_sample)
endif()
