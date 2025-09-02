file(REMOVE_RECURSE
  "bandicam/qml/Main.qml"
  "bandicam/qml/cuttingpage.qml"
  "bandicam/qml/settingpage.qml"
  "bandicam/qml/videopage.qml"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/qml_module_tooling.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
