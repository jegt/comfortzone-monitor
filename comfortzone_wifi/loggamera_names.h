#pragma once
#include <string.h>

namespace comfortzone {
// Exact Influx name tags confirmed by the 2026-09-19 field comparison.
// These are compatibility names, not translations or value conversions.
inline const char *loggameraName(const char *id) {
  static const struct { const char *id; const char *name; } names[] = {
    {"outdoor", "Utomhustemperatur (TE0)"},
    {"indoor", "Inomhustemperatur (TE3)"},
    {"hot_water", "Varmvattentemperatur (TE24)"},
    {"flow", "Framledning (TE1)"},
    {"return", "Returledning (TE2)"},
    {"hot_gas", "Hetgastemperatur TE4"},
    {"evaporator_out", "Förångare ut (TE5)"},
    {"evaporator_in", "Förångartemperatur in TE6"},
    {"exhaust_air", "Avluft temperatur TE7"},
    {"compressor_frequency", "Kompressorfrekvens"},
    {"compressor_power", "Kompressoreffekt"},
    {"additional_power", "Tillskottseffekt"},
    {"room_setpoint", "Inomhustemperatur inställt"},
    {"hot_water_setpoint", "Varmvatten inställt"},
    {"heating_curve", "Värmekurva"},
    {"fan_mode", "Fläktläge"},
    {"holiday_days", "Semestersänkning (dagar)"},
    {"minimum_return", "Minsta returtemp"},
    {"additional_heat_threshold", "Eltillskott tillåtet"},
    {"filter_days", "Filterbytesintervall"},
    {"heating_valve", "Växelventil värme (av/på)"},
    {"hot_water_valve", "Växelventil varmvatten (av/på)"},
    {"hot_water_priority_code", "Varmvatten prioritet"},
    {"extra_hot_water_code", "Extra varmvatten"},
    {"filterchangealarm", "Filterlarm (av/på)"},
    {"compressor_running", "Kompressor aktiv"},
    {"compressor_power_heating", "Kompressoreffekt värme"},
    {"compressor_power_hot_water", "Kompressoreffekt varmvatten"},
    {"additional_power_heating", "Tillskottseffekt värme"},
    {"additional_power_hot_water", "Tillskottseffekt varmvatten"},
    {"curve_10", "Kurvförskjutning ut=10"},
    {"curve_5", "Kurvförskjutning ut=5"},
    {"curve_0", "Kurvförskjutning ut=0"},
    {"curve_m5", "Kurvförskjutning ut=-5"},
    {"curve_m10", "Kurvförskjutning ut=-10"},
    {"curve_m15", "Kurvförskjutning ut=-15"},
  };
  for (const auto &entry : names) {
    if (strcmp(id, entry.id) == 0) return entry.name;
  }
  return nullptr;
}
}
