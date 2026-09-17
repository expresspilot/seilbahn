import re

with open("index.html", "r") as f:
    html = f.read()

c_code = f"""#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
{html})rawliteral";

#endif
"""

with open("ast_controller/include/WebUI.h", "w") as f:
    f.write(c_code)

#with open("gst_controller/include/WebUI.h", "w") as f:
#    f.write(c_code)

print("Successfully packed index.html into ast_controller/include/WebUI.h")
