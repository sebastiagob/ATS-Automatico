#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <LiquidCrystal_I2C.h>

Adafruit_ADS1115 ads1115;  // Primer ADS1115
Adafruit_ADS1015 ads1015;  // Segundo ADS1015

LiquidCrystal_I2C lcd(0x27, 20, 4);  // Dirección I2C del LCD 16x2

const float FACTOR = 30; // 30A/1V
const float multiplier_1115 = 0.0625F; // ADS1115: 1 bit = 0.0625mV
const float multiplier_1015 = 0.625F;  // ADS1015: 1 bit = 0.1875mV

// Pines de los relés
const int relayPins[] = {14, 27, 26, 25, 33};
const int numRelays = 5;

float corrientes1[10]; // Array para almacenar los últimos 10 valores de corriente del primer ADS1115
float corrientes2[10]; // Array para almacenar los últimos 10 valores de corriente del segundo ADS1115
int indice = 0;       // Índice para los arrays de corrientes
bool arranque = false;

void ImprimirMedidas(String prefix, float value, String postfix);
void controlRelays(float promedioCorriente);

void setup()
{
  Serial.begin(115200);  // Ajustar la velocidad de baudios para el ESP32

  // Inicializar los pines de los relés como salidas y establecerlos en HIGH
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  // Inicializar ADS1115 y ADS1015 con las direcciones I2C y los pines especificados
  ads1115.setGain(GAIN_TWO); // ±2.048V  1 bit = 0.0625mV
  ads1015.setGain(GAIN_TWO); // ±2.048V  1 bit = 0.1875mV
  
  if (!ads1115.begin(0x48)) {
    Serial.println("¡No se encontró un ADS1115!");
    //while (1);
  }
  
  if (!ads1015.begin(0x49)) {
    Serial.println("¡No se encontró un ADS1015!");
    while (1);
  }

  // Inicializar el array de corrientes con ceros
  for (int i = 0; i < 10; i++) {
    corrientes1[i] = 0.0;
    corrientes2[i] = 0.0;
  }

  // Inicializar LCD
  lcd.begin(16, 2);  // Iniciar el LCD con 16 columnas y 2 filas
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("Red Presente");
  lcd.setCursor(0, 2);
  lcd.print("Motor Ausente");
}

void loop()
{
  float CorrienteRMS1 = getCorriente(ads1115, multiplier_1115);
  float CorrienteRMS2 = getCorriente(ads1015, multiplier_1015);
  //float CorrienteRMS2 = 0.005;
  
  // Almacenar los valores de corriente RMS en los arrays
  corrientes1[indice] = CorrienteRMS1;
  corrientes2[indice] = CorrienteRMS2;
  indice = (indice + 1) % 10; // Incrementar el índice y mantenerlo en el rango de 0-9

  // Calcular el promedio de los últimos 10 valores
  float promedioCorriente1 = 0.0;
  float promedioCorriente2 = 0.0;
  for (int i = 0; i < 10; i++) {
    promedioCorriente1 += corrientes1[i];
    promedioCorriente2 += corrientes2[i];
  }
  promedioCorriente1 /= 10.0;
  promedioCorriente2 /= 10.0;

  ImprimirMedidas("Irms Promedio1: ", promedioCorriente1, "A ,");
  ImprimirMedidas("Irms Promedio2: ", promedioCorriente2, "A ,");

  static bool lastState = (promedioCorriente1 >= 0.010);

  // Llamar a la función de control de relés solo si hay un cambio en el estado
  bool currentState = (promedioCorriente1 >= 0.010);
  if (currentState != lastState) {
    controlRelays(promedioCorriente1);
    lastState = currentState;
  }

  // Actualizar la pantalla LCD según las condiciones
  lcd.clear();
  if (arranque) {

    lcd.setCursor(0, 0);
    lcd.print("    Corte de Luz    ");
    lcd.setCursor(0, 2);
    lcd.print("El motor no arranca ");

    if (CorrienteRMS2 > 0.01 || CorrienteRMS1 > 0.01) {
      arranque = false;
    }

  } else {
      if (promedioCorriente1 >= 0.010) {

      lcd.setCursor(0, 0);
      lcd.print("Red Presente        ");

      lcd.setCursor(0, 1);
      lcd.print("Corriente: ");
      lcd.print(promedioCorriente1, 3);
      lcd.print("A   ");

      lcd.setCursor(0, 2);
      lcd.print("Motor Ausente       ");

    } else {

      lcd.setCursor(0, 0);
      lcd.print("Motor Presente      ");

      lcd.setCursor(0, 1);
      lcd.print("Corriente: ");
      lcd.print(promedioCorriente2, 3);
      lcd.print("A   ");

      lcd.setCursor(0, 2);
      lcd.print("Red Ausente         ");    

    }
  }

}

void ImprimirMedidas(String prefix, float value, String postfix)
{
  Serial.print(prefix);
  Serial.print(value, 3);
  Serial.println(postfix);
}

float getCorriente(Adafruit_ADS1X15 ads, float multiplier)
{
  float Volt_diferencial;
  float corriente;
  float sum = 0;
  long tiempo = millis();
  int counter = 0;

  while (millis() - tiempo < 1000)
  {
    Volt_diferencial = ads.readADC_Differential_0_1() * multiplier;
    //-----------------------------------------------------
    //Volt_diferencial = ads.readADC_Differential_2_3() * multiplier;
    //-----------------------------------------------------
    corriente = Volt_diferencial * FACTOR;
    corriente /= 1000.0;

    sum += sq(corriente);
    counter = counter + 1;
  }

  corriente = sqrt(sum / counter);
  return corriente;
}

void controlRelays(float promedioCorriente)
{
  if (promedioCorriente < 0.010) {

    lcd.setCursor(0, 0);
    lcd.print("    Corte de Luz    ");
    lcd.setCursor(0, 1);
    lcd.print("                    ");
    lcd.setCursor(0, 2);
    lcd.print("Arrancando Motor    ");
    
    digitalWrite(relayPins[0], LOW);
    digitalWrite(relayPins[1], LOW);
    digitalWrite(relayPins[3], LOW);

    int tiempo_arranque = 2500;
    String mensaje = "Arrancando Motor ";
    arranque = true;

    for (int i = 0; i < 3; i++) {
      mensaje += ".";
      lcd.setCursor(0, 2);
      lcd.print(mensaje);    

      digitalWrite(relayPins[2], LOW);
      delay(tiempo_arranque);  // Mantener el relé 3 en LOW por 5 segundos
      digitalWrite(relayPins[2], HIGH);

      float CorrienteRMS2 = getCorriente(ads1015, multiplier_1015);
      tiempo_arranque += 500;

      if (CorrienteRMS2 >= 0.01) {
      lcd.setCursor(0, 2);
      lcd.print("  Arranque Exitoso  ");  
      delay(2500);  // Mantener el relé 3 en LOW por 5 segundos
      digitalWrite(relayPins[4], LOW);
      arranque = false;
      break;
      }
    }

  } else {

    lcd.setCursor(0, 0);
    lcd.print("   Regreso de Luz   ");
    lcd.setCursor(0, 1);
    lcd.print("                    ");
    lcd.setCursor(0, 2);
    lcd.print("   Motor Detenido   ");

    for (int i = 0; i < 2; i++) {
      digitalWrite(relayPins[i], HIGH);
    }

    digitalWrite(relayPins[4], HIGH);
    delay(5000);  // Mantener el relé 3 en LOW por 5 segundos
    digitalWrite(relayPins[3], HIGH);

  }
} 