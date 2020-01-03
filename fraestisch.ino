#include <Encoder.h>
#include <Wire.h> // Wire Bibliothek einbinden
#include <LiquidCrystal_I2C.h> // Vorher hinzugefügte LiquidCrystal_I2C Bibliothek einbinden



/*
 * Grobe Funktionsweise
 * 
 * 1. Wenn Strom an dann nix machen sondern im Display fragen "Referenzfahrt? Auf keine Encodereingaben reagieren, nur auf Klick warten.
 * 2. So lange warten bis Encoder gedrückt wird, vorher darf keine Bewegung möglich sein!!! Dann Referenzfahrt auf Endschalter.
 * 3. Jetzt ist die obere Position bekannt (max. Verfahrweg ca. 5cm, mussnoch ermittelt werden)
 * 4. wenn Encoder gedreht wird dann pro Rastung 0,1 mm in die entsprechende Richtung fahren (schnell oder langsam, je nach dem wie schnell gedreht wird 
 * 5. wenn Encoder länger gedrückt wird (1  sec) Fräse in maximale  untere Position  fahren  (wenn Frästisch nicht gebraucht  wird)
 * PS: Encoder macht 20 Klicks pro Umdrehung, Steigung einer M8 Gewindestange = 1,25 mm
 *
 */


/*
 * Anschlüsse am Ardunino
 */

int PUL=8; //define Pulse pin
int DIR=6; //define Direction pin
int ENA=5; //define Enable Pin

int CLK=0; // Clock f. Encoder
int DT=1;  // DT f. Encoder
int SW=7;  // Switch vom. Encoder

int DISPLAY_SDA=2;  // I2C-SDA fuers Display
int DISPLAY_SCL=3;  // I2C-SCL fuers Display

int ENDSTOP=4; // Endschalter fuer die Referenz


/*
 * State-Machine für die Referenzierung
 */

#define STATE_UNREFERENZIERT    0
#define STATE_REFERENZFAHRT     1
#define STATE_REFERENZIERT      2

int state = STATE_UNREFERENZIERT;




/*
 * Der Knopf wurde im Interrupt-Handler gedrückt
 */
boolean button_gedrueckt = false;



/*
 * Position im Raum
 * 
 *   x
 *   | ist_position+
 *   |
 *   |
 *   | ist_position-
 *   x
 *   
 */
long ist_position = 0;
long soll_position = 0;

double ist_position_mm = 0;
double soll_position_mm = 0;

long maximale_position = 50;
boolean position_geaendert = false;

long STEPS_PRO_MM = 2560;

/*
 * 
 */

// Encoder
Encoder encoder(CLK, DT);

// Display
LiquidCrystal_I2C lcd(0x27, 16, 2); //Hier wird festgelegt um was für einen Display es sich handelt. In diesem Fall eines mit 16 Zeichen in 2 Zeilen und der HEX-Adresse 0x27. Für ein vierzeiliges I2C-LCD verwendet man den Code "LiquidCrystal_I2C lcd(0x27, 20, 4)" 


long alteEncoderPosition  = -999;


// Knopf gedrückt
void encoderGedrueckt() 
{
  Serial.println("Switch betaetigt");
  button_gedrueckt = true;
}

void motor_schritt_oben() 
{
    digitalWrite(DIR,LOW);
    digitalWrite(ENA,HIGH);
    digitalWrite(PUL,HIGH);
    delayMicroseconds(50);
    digitalWrite(PUL,LOW);
    delayMicroseconds(50);

    ist_position++;
    position_geaendert = true;
}


void motor_schritt_unten()
{
    digitalWrite(DIR,HIGH);
    digitalWrite(ENA,HIGH);
    digitalWrite(PUL,HIGH);
    delayMicroseconds(50);
    digitalWrite(PUL,LOW);
    delayMicroseconds(50);

    ist_position--;
    position_geaendert = true;
}


// Setup
void setup() {
  
  pinMode (PUL, OUTPUT);
  pinMode (DIR, OUTPUT);
  pinMode (ENA, OUTPUT);

  // Pullup für den Drehschalter-Knopf
  pinMode(SW, INPUT);
  digitalWrite(SW, HIGH);

  // Pullup für den Encoder
  pinMode(CLK, INPUT);
  digitalWrite(CLK, HIGH);
  pinMode(DT, INPUT);
  digitalWrite(DT, HIGH);

  // Pullup für den Endschalter
  pinMode(ENDSTOP, INPUT);
  digitalWrite(ENDSTOP, HIGH);

  // LCD Initialisieren
  lcd.init(); //Im Setup wird der LCD gestartet 
  lcd.backlight(); //Hintergrundbeleuchtung einschalten (lcd.noBacklight(); schaltet die Beleuchtung aus). 

  // Enable Serial
  Serial.begin(9600);
//
//  // Wait for begin
//  while (! Serial);

  // Interrupt an die Methode binden
  attachInterrupt(digitalPinToInterrupt(SW), encoderGedrueckt, FALLING);

  // LCD schreiben
  lcd.setCursor(0, 0); 
  lcd.print("i=      s=      "); 
  lcd.setCursor(0, 1);
  lcd.print("encoder =       ");

}



// Endlosschleife
int display_refresh = 10240;
void loop() {

  // Encoder lesen
  long neueEncoderPosition;
  neueEncoderPosition = encoder.read();

  if (neueEncoderPosition != alteEncoderPosition) {
    Serial.print("encoder = ");
    Serial.print(neueEncoderPosition);
    Serial.println();

    soll_position = soll_position + ((neueEncoderPosition - alteEncoderPosition) * 640);
    
    alteEncoderPosition = neueEncoderPosition;

    lcd.setCursor(10, 1);
    lcd.print("      "); 
    lcd.setCursor(10, 1);
    lcd.print(neueEncoderPosition);

    position_geaendert = true;

  }

  // State-Machine
  switch( state ) {


    // Unreferenzierte Maschine, Auf keine Encodereingaben reagieren, nur auf Klick warten. Danach die Referenzfahrt starten.
    case STATE_UNREFERENZIERT:

      if (button_gedrueckt == true) {
        state = STATE_REFERENZFAHRT;
        Serial.println("State STATE_UNREFERENZIERT > STATE_REFERENZFAHRT");
        button_gedrueckt = false;
      }
      
      break;



    // Refernzfahrt. Solange mit dem Portal nach unten Fahren, bis der Referenzschalter getroffen wurde.
    case STATE_REFERENZFAHRT:

      if (digitalRead(ENDSTOP) == HIGH) {
        motor_schritt_oben();
      } else {
        state = STATE_REFERENZIERT;
        soll_position = ist_position;
        Serial.println("State STATE_REFERENZFAHRT > STATE_REFERENZIERT");
        position_geaendert = true;
      }
  
      break;



    // Maschine referenziert.
    case STATE_REFERENZIERT:

      // soll/ist nullen bei knopfdruck
      if (button_gedrueckt == true) {
        ist_position = 0;
        soll_position = ist_position;
        position_geaendert = true;
        button_gedrueckt = false;
      }

      // nach oben
      if (soll_position > ist_position) {
        if (digitalRead(ENDSTOP) == HIGH) {
          motor_schritt_oben();
        }
      }
      // nach unten
      if (soll_position < ist_position) {
        motor_schritt_unten();
      }
    
      break;

    
  }


  display_refresh--;
  if (display_refresh < 1) 
  {

    // Position auf dem Display
    if (position_geaendert == true) 
    {

      // mm berechnen
      soll_position_mm = ((double) soll_position) / ((double) STEPS_PRO_MM );
      ist_position_mm  = ((double) ist_position)  / ((double) STEPS_PRO_MM );
  
      lcd.setCursor(2, 0);
      lcd.print("      "); 
      lcd.setCursor(2, 0);
      lcd.print(ist_position_mm);
  
      lcd.setCursor(10, 0);
      lcd.print("      "); 
      lcd.setCursor(10, 0);
      lcd.print(soll_position_mm);
      
      position_geaendert = false;
      
    }
    
    display_refresh = 10240;
  
  }
  
}
