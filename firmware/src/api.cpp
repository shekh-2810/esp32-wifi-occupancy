#include "api.h"

#include <WebServer.h>
#include <WiFi.h>

#include "buzzer.h"
#include "clap.h"
#include "config.h"
#include "display.h"
#include "sniffer.h"

namespace api {
namespace {

WebServer server(80);
bool serverStarted = false;

// ---- Minimal parser for the server's small, fixed JSON payload ----

bool extractString(const String& body, const String& key, String& out) {
    int idx = body.indexOf("\"" + key + "\"");
    if (idx < 0) return false;
    int colon = body.indexOf(':', idx);
    if (colon < 0) return false;
    int q1 = body.indexOf('"', colon + 1);
    if (q1 < 0) return false;
    int q2 = body.indexOf('"', q1 + 1);
    if (q2 < 0) return false;
    out = body.substring(q1 + 1, q2);
    return true;
}

bool extractNumber(const String& body, const String& key, double& out) {
    int idx = body.indexOf("\"" + key + "\"");
    if (idx < 0) return false;
    int colon = body.indexOf(':', idx);
    if (colon < 0) return false;
    int i = colon + 1;
    while (i < (int)body.length() && body[i] == ' ') i++;
    int start = i;
    while (i < (int)body.length() && (isDigit(body[i]) || body[i] == '-' || body[i] == '.')) i++;
    if (i == start) return false;
    out = body.substring(start, i).toDouble();
    return true;
}

// ---- Handlers ----

String buildStatusJson() {
    String json = "{";
    json += "\"node\":\"" NODE_ID "\",";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += sniffer::statusFields() + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"display_mode\":\"" + display::mode() + "\",";
#ifdef ROLE_MIC
    json += "\"clap_count\":" + String(clap::count()) + ",";
    json += "\"mic_ao\":" + String(clap::lastAnalog()) + ",";
    json += "\"mic_do\":" + String(clap::digitalLevel()) + ",";
#endif
    json += "\"uptime_ms\":" + String(millis());
    json += "}";
    return json;
}

void handleActuatorPost() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing JSON body");
        return;
    }
    String body = server.arg("plain");

    String mode;
    if (!extractString(body, "mode", mode)) {
        server.send(400, "text/plain", "Expected {\"mode\": \"...\"}");
        return;
    }

    double count = 0, scale = 1.0;
    extractNumber(body, "count", count);  // optional, depends on mode
    extractNumber(body, "scale", scale);  // optional, "area" mode only

    display::setState(mode, (int)count, (float)scale);
    buzzer::announceMode(mode);  // no-op unless this is the buzzer board

    server.send(200, "text/plain", "OK");
}

void handleRoot() {
    String html =
        "<html><body style='font-family:monospace;background:#111;color:#0f0;padding:20px'>"
        "<h2>" NODE_ID " -- occupancy node</h2>"
        "<p>See <a style='color:#4fd8e8' href='/api/status'>/api/status</a>, "
        "<a style='color:#4fd8e8' href='/api/devices'>/api/devices</a></p></body></html>";
    server.send(200, "text/html", html);
}

}  // namespace

void begin() {
    if (WiFi.status() != WL_CONNECTED) return;

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, []() {
        server.send(200, "application/json", buildStatusJson());
    });
    server.on("/api/devices", HTTP_GET, []() {
        server.send(200, "application/json", sniffer::devicesJson(NODE_ID));
    });
    server.on("/api/actuator", HTTP_POST, handleActuatorPost);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
    serverStarted = true;

    Serial.print("Web server up: http://");
    Serial.println(WiFi.localIP());
}

bool started() { return serverStarted; }

void handleClient() {
    if (serverStarted) server.handleClient();
}

}  // namespace api
