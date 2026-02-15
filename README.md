west build -p always -b nucleo_f401re workspace/vl53l5cx/samples/vl53l5cx/ -- -DZEPHYR_EXTRA_MODULES=$(pwd)/workspace/vl53l5cx -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
