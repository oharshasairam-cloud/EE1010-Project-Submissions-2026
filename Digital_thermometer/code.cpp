#include <LiquidCrystal.h>

// =====================================================
//  SETTINGS 
// =====================================================

// LCD pins (RS on D7, E on D6, data on D5..D2)
const int LCD_RS = 7;
const int LCD_E  = 6;
const int LCD_D4 = 5;
const int LCD_D5 = 4;
const int LCD_D6 = 3;
const int LCD_D7 = 2;

// Sensor circuit: 5V - R1 - junction - PT100 - GND, junction goes to A0
const int   SENSOR_PIN   = A0;
const float R1           = 100.0;   // series resistor in ohms
const int   NUM_READINGS = 64;      // readings averaged to reduce noise

// Calibration points: PT100 resistance (ohm) and the temperature (deg C)
// Standard values from the IEC 60751 PT100 table.
// After water-bath calibration, replace them with your own measured values.
const int NUM_POINTS = 6;
float resistancePoints[NUM_POINTS]  = {100.00, 107.79, 115.54, 123.24, 130.89, 138.51};
float temperaturePoints[NUM_POINTS] = {0.0, 20.0, 40.0, 60.0, 80.0, 100.0};

// Scaling used in the fit: u = (R - R_CENTER) / R_SPAN
// Keeps the numbers small so the maths stays accurate.
const float R_CENTER = 120.0;
const float R_SPAN   = 20.0;

// =====================================================
//  END OF SETTINGS
// =====================================================


LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Fitted model: temperature = coeffA*u*u + coeffB*u + coeffC
float coeffA = 0, coeffB = 0, coeffC = 0;
bool  fitOk  = false;               // true only if the fit looks sane


// Resistance shifted to the middle of our range and scaled
float scaleResistance(float resistance) {
  return (resistance - R_CENTER) / R_SPAN;
}

// Determinant of a 3x3 matrix
float determinant3x3(float m[3][3]) {
  return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
       - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
       + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

// Cramer's rule: replace one column with the right-hand side, take the determinant
float determinantWithColumnReplaced(float m[3][3], float rhs[3], int column) {
  float copy[3][3];
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      copy[row][col] = m[row][col];
    }
    copy[row][column] = rhs[row];
  }
  return determinant3x3(copy);
}

float resistanceToTemperature(float resistance) {
  float u = scaleResistance(resistance);
  return coeffA * u * u + coeffB * u + coeffC;
}

// Least squares quadratic fit through the calibration points
void calculateQuadraticFit() {
  float sumU = 0, sumU2 = 0, sumU3 = 0, sumU4 = 0;
  float sumT = 0, sumUT = 0, sumU2T = 0;

  for (int i = 0; i < NUM_POINTS; i++) {
    float u = scaleResistance(resistancePoints[i]);
    float t = temperaturePoints[i];
    sumU   += u;
    sumU2  += u * u;
    sumU3  += u * u * u;
    sumU4  += u * u * u * u;
    sumT   += t;
    sumUT  += u * t;
    sumU2T += u * u * t;
  }

  // Normal equations: matrix * [a b c] = rhs
  float matrix[3][3] = {
    {sumU4, sumU3, sumU2},
    {sumU3, sumU2, sumU},
    {sumU2, sumU,  (float)NUM_POINTS}
  };
  float rhs[3] = {sumU2T, sumUT, sumT};

  float mainDeterminant = determinant3x3(matrix);
  if (fabs(mainDeterminant) < 0.000001) {   // bad points (e.g. repeated values)
    fitOk = false;
    return;
  }
  coeffA = determinantWithColumnReplaced(matrix, rhs, 0) / mainDeterminant;
  coeffB = determinantWithColumnReplaced(matrix, rhs, 1) / mainDeterminant;
  coeffC = determinantWithColumnReplaced(matrix, rhs, 2) / mainDeterminant;

  // Fit check: curve must pass within 5 deg C of every calibration point
  fitOk = true;
  for (int i = 0; i < NUM_POINTS; i++) {
    float error = resistanceToTemperature(resistancePoints[i]) - temperaturePoints[i];
    if (isnan(error) || fabs(error) > 5.0) fitOk = false;
  }
}

// Average of many A0 readings (0 to 1023)
float readAverageAdc() {
  long total = 0;
  for (int i = 0; i < NUM_READINGS; i++) {
    total += analogRead(SENSOR_PIN);
    delay(2);
  }
  return (float)total / NUM_READINGS;
}

// ADC = 1024 * Rpt / (R1 + Rpt)  =>  Rpt = R1 * ADC / (1024 - ADC)
float adcToResistance(float adcValue) {
  return R1 * adcValue / (1024.0 - adcValue);
}


void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);

  calculateQuadraticFit();

  // Banner: shows which version is running
  lcd.print("PT100 Thermo v7");
  lcd.setCursor(0, 1);
  lcd.print(fitOk ? "Fit OK" : "Fit ERROR");
  delay(1500);
  lcd.clear();

  // Print the fit on Serial so we can check it
  Serial.println("--- PT100 v7 ---");
  Serial.print("a="); Serial.print(coeffA, 4);
  Serial.print("  b="); Serial.print(coeffB, 4);
  Serial.print("  c="); Serial.println(coeffC, 4);
}

void loop() {
  float adcValue = readAverageAdc();

  // Line 1: ADC value, always shown (even when there is an error)
  lcd.setCursor(0, 0);
  lcd.print("ADC=");
  lcd.print(adcValue, 1);
  lcd.print("        ");              // spaces wipe leftover characters

  // Line 2: temperature, or a short error message
  lcd.setCursor(0, 1);
  if (!fitOk) {
    lcd.print("T= fit error    ");
  }
  // ADC near 0 = PT100 shorted, near 1023 = PT100 disconnected
  else if (adcValue < 2) {
    lcd.print("T= short (ADC~0)");
  }
  else if (adcValue > 1021) {
    lcd.print("T= open (ADC~1k)");
  }
  else {
    float resistance  = adcToResistance(adcValue);
    float temperature = resistanceToTemperature(resistance);
    lcd.print("T=");
    lcd.print(temperature, 1);
    lcd.write(223);                 // degree symbol
    lcd.print("C       ");           // spaces wipe leftover characters
  }

  // Serial output every loop, used for calibration
  Serial.print("ADC="); Serial.print(adcValue, 2);
  if (adcValue > 0 && adcValue < 1023) {
    float r = adcToResistance(adcValue);
    Serial.print("  R="); Serial.print(r, 2);
    Serial.print(" ohm  T="); Serial.print(resistanceToTemperature(r), 2);
    Serial.println(" C");
  } else {
    Serial.println("  (out of range)");
  }

  delay(500);
}
