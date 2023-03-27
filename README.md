# esp-idf-sketches
A repository containing small projects for the ESP32 built using the Espressif sdk.

The documentation for the Espressif ESP32 sdk can be found at:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/

To upload and run the sketches that are here, jump to step 5 in the typical workflow.

Typical Workflow:
1. Open esp-idf powershell or command prompt
2. Navigate to working directory
3. Execute "idf.py create-project [project_name]"
4. navigate to newly created project
5. Execute "idf.py set-target [esp-target, default=esp32]"
6. Write code in main/{project_name}.c
7. Execute "idf.py build"
8. Execute "idf.py -p [port] flash monitor"
