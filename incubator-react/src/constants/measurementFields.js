export const MEASUREMENT_FIELDS = [
    { key: "temperature_celsius", label: "Temperature", unit: "°C", kind: "numeric" },
    { key: "humidity_rh", label: "Humidity", unit: "%", kind: "numeric" },
    { key: "co2_ppm", label: "CO₂", unit: "ppm", kind: "numeric" },
    { key: "weight_gram", label: "Weight", unit: "g", kind: "numeric" },
    { key: "light_intensity", label: "Light", unit: "lux", kind: "numeric" },
    { key: "sound_intensity", label: "Sound", unit: "dB", kind: "numeric" },
    { key: "voltage", label: "Voltage", unit: "V", kind: "numeric" },
    { key: "current", label: "Current", unit: "A", kind: "numeric" },
    { key: "water_level", label: "Water Level", unit: "%", kind: "numeric" },
    { key: "pitch_deg", label: "Pitch", unit: "°", kind: "numeric" },
    { key: "roll_deg", label: "Roll", unit: "°", kind: "numeric" },
    { key: "relay_state_1", label: "Relay 1", unit: "", kind: "boolean" },
    { key: "relay_state_2", label: "Relay 2", unit: "", kind: "boolean" },
    { key: "relay_state_3", label: "Relay 3", unit: "", kind: "boolean" },
    { key: "relay_state_4", label: "Relay 4", unit: "", kind: "boolean" },
    { key: "humidifier_state", label: "Humidifier", unit: "", kind: "boolean" },
];

export const MEASUREMENT_FIELDS_BY_KEY = Object.fromEntries(
    MEASUREMENT_FIELDS.map(field => [field.key, field])
);

export const DEFAULT_VISIBLE_FIELDS = ["temperature_celsius", "humidity_rh", "co2_ppm"];
