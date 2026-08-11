import re

with open("AST_esp_code/index.html", "r") as f:
    html = f.read()

c_code = f"""#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
{html})rawliteral";

#endif
"""

with open("AST_esp_code/WebUI.h", "w") as f:
    f.write(c_code)

with open("GST_esp_code/WebUI.h", "w") as f:
    f.write(c_code)

print("Successfully packed index.html into AST_esp_code/WebUI.h and GST_esp_code/WebUI.h")
