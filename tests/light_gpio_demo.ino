/*
 * Smart Light GPIO Hardware Demo
 *
 * Purpose:
 * Demonstrate that the ESP32 can control the physical
 * light/relay through a GPIO pin.
 *
 * The light turns ON for 3 seconds,
 * then OFF for 3 seconds repeatedly.
 *
 * Confirm the relay GPIO pin before testing.
 */

// Current smart-light firmware uses GPIO 5.
// Change this if the hardware uses a different GPIO.
const int LIGHT_PIN = 5;

const unsigned long DEMO_DELAY = 3000;

void setup() {
    // Start Serial Monitor for demo/debug output
    Serial.begin(115200);

    // Configure relay control pin
    pinMode(LIGHT_PIN, OUTPUT);

    // Start with the light OFF
    digitalWrite(LIGHT_PIN, LOW);

    Serial.println();
    Serial.println("Smart Light Hardware Demo");
    Serial.println("-------------------------");
    Serial.println("GPIO initialized.");
    Serial.println("Starting ON/OFF demo...");
}

void loop() {
    // Turn the light ON
    digitalWrite(LIGHT_PIN, HIGH);
    Serial.println("Light ON");

    delay(DEMO_DELAY);

    // Turn the light OFF
    digitalWrite(LIGHT_PIN, LOW);
    Serial.println("Light OFF");

    delay(DEMO_DELAY);
}