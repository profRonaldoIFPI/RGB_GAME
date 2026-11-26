// Configuração da interface web. TROQUE a senha e o PIN antes do evento!
#pragma once

constexpr char WIFI_SSID[] = "RGB-GAME";     // nome da rede criada pelo ESP32
constexpr char WIFI_PASS[] = "arcade2026";   // senha da rede (8 caracteres ou mais)
constexpr char ADMIN_PIN[] = "1234";         // PIN da página http://192.168.4.1/admin

static_assert(sizeof(WIFI_PASS) - 1 >= 8, "A senha do Wi-Fi precisa de 8 caracteres ou mais");
