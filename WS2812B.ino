#include <FastLED.h>

#define NUM_LEDS 21
#define NUM_COLORS 6

#define LED_PIN 7
#define PRIMARY_SENSOR_PIN 3
#define SECONDARY_SENSOR_PIN 2

#define INTERRUPT_COOLDOWN 50000
#define WHEEL_DIAMETER 0.205
#define WHEEL_CIRCUMFERENCE 3.14159 * WHEEL_DIAMETER
#define LEDS_PER_METER 60
#define LEDS_PER_REVOLUTION LEDS_PER_METER * WHEEL_CIRCUMFERENCE

CRGB leds[NUM_LEDS];

CRGB colors[NUM_COLORS] = {0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF};
int color_idx = 0;

volatile double rps = 0.7; // revolutions per second
double current_position = 0;
unsigned long last_frame_micros = 0;

volatile unsigned long last_magnet_pass = 0;
volatile unsigned long last_interrupt_time = 0;

volatile boolean secondary_sensor_value = false;

void setup() {
  pinMode(PRIMARY_SENSOR_PIN, INPUT);
  pinMode(SECONDARY_SENSOR_PIN, INPUT);

  attachInterrupt(1, primary_sensor_fired, RISING); // 1 --> interrupt vector 1 == PIN 3
  attachInterrupt(0, secondary_sensor_fired, CHANGE); // 0 --> interrupt vector 0 == PIN 2

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
}

void primary_sensor_fired () {
  // debounce
  if (micros() - last_interrupt_time > INTERRUPT_COOLDOWN) {
    // 1000000 us per second / 5 magnet passes per revolution
    rps = 200000.0 / (micros() - last_magnet_pass);

    if (!secondary_sensor_value) {
      rps = -rps;
    }
    
    last_magnet_pass = micros();
  }

  last_interrupt_time = micros();
}

void secondary_sensor_fired () {
  if (digitalRead(SECONDARY_SENSOR_PIN) == HIGH) {
    secondary_sensor_value = true;
  } else {
    secondary_sensor_value = false;
  }
}

int fade_in_fade_out (CRGB color, int led_idx) {
  if (led_idx == 0 || led_idx == NUM_LEDS - 1) {
    // first or last led: 50% brightness to fade in/out
    return color ^ 0x7F7F7F;
  } else if (led_idx == 1 || led_idx == NUM_LEDS - 2) {
    // second or second last led: 75% brigntess to fade in/out
    return color ^ 0xBFBFBF;
  } else {
    return color;
  }
}

void loop() {
  // anti-stall
  if (micros() - last_interrupt_time > 1000000) {
    rps = 0.0;
  }
  
  unsigned long current_micros = micros();
  unsigned long micros_diff = current_micros - last_frame_micros;
  last_frame_micros = current_micros;

  double movement = micros_diff * rps * LEDS_PER_REVOLUTION / 1000000.0;
  current_position += movement;

  // one cycle done, switch colors
  if (current_position >= NUM_LEDS) {
    color_idx = (color_idx + 1) % NUM_COLORS;
    current_position -= ((int) current_position / NUM_LEDS) * NUM_LEDS;
  }

  if (current_position < 0) {
    color_idx = (color_idx + NUM_COLORS - 1) % NUM_COLORS;
    current_position -= ((int) current_position / NUM_LEDS) * NUM_LEDS;
    current_position += NUM_LEDS;
  }

  // quadratic fading between pixels
  double current_pixel_brightness = 1.0 - (current_position - (int) current_position);
  current_pixel_brightness = 255.0 * current_pixel_brightness * current_pixel_brightness;
  double next_pixel_brightness = current_position - (int) current_position;
  next_pixel_brightness = 255.0 * next_pixel_brightness * next_pixel_brightness;

  for (int idx = 0; idx < NUM_LEDS; idx++) {
    if (idx == (int) current_position) {
      leds[idx] = colors[color_idx];
      leds[idx] %= (int) current_pixel_brightness;
    } else if (idx == ((int) current_position + 1) % NUM_LEDS) {
      leds[idx] = colors[color_idx];
      leds[idx] %= (int) next_pixel_brightness;
    } else {
      leds[idx] = CRGB::Black;
    }

    leds[idx] = fade_in_fade_out(leds[idx], idx);
  }



  FastLED.show();
  delay(20);
}
