# Sistem de control al iluminatului inteligent (Hardware-Software Co-design pe Zynq)

## 📌 Descrierea Proiectului
Acest proiect implementează un sistem de control al iluminatului inteligent pe o platformă Zynq (Xilinx Vivado + Vitis). Sistemul ajustează automat intensitatea luminii în funcție de nivelul de iluminare ambientală folosind un senzor de lumină citit prin XADC și permite, de asemenea, control manual prin intermediul unei tastaturi matriciale Pmod Keypad.

## 🛠️ Arhitectură și Tehnologii (Co-design)
Proiectul folosește capabilitățile SoC ale platformei Zynq, separând logica hardware (PL - Programmable Logic) de cea software (PS - Processing System):

* **Partea de Hardware (VHDL & Block Design):**
  * Modul VHDL customizat pentru generarea semnalului PWM prin compararea unui contor cu valoarea duty.
  * Integrarea modulelor IP XADC pentru conversia analog-digitală a senzorului de lumină (citit pe canalele VAUX).
  * Interconectare prin magistrala AXI GPIO pentru comunicarea cu perifericele. Sunt controlate 4 LED-uri standard (LD0-LD3) prin PWM și un LED RGB (LRGB) pentru scene.

* **Partea de Software (C în Vitis):**
  * Implementarea algoritmului de tip polling (activarea pe rând a coloanelor și citirea rândurilor) pentru scanarea tastaturii matriciale.
  * Mașină de stări (`State Machine`) bazată pe un `enum` (`SceneType`) pentru gestionarea fluidă a scenelor și a efectelor vizuale.

## ⚙️ Moduri de Funcționare

Sistemul este controlat de un Switch principal (SW0) care alternează între cele 2 moduri:

1. **Modul AUTO (SW0 = 0):**
   * Sistemul ajustează singur intensitatea în funcție de lumina ambientală. La o lumină ambientală scăzută se crește intensitatea (factorul de umplere PWM mai mare), iar la lumină ridicată se scade intensitatea sau se oprește iluminarea.

2. **Modul MANUAL (SW0 = 1):**
   * Permite utilizatorului să aleagă nivelul de intensitate sau scena dorită folosind tastatura matricială. Modurile implementate sunt:
   * **Tasta 0 (`SCENE_EMPTY`):** Modul oprit, toate LED-urile sunt stinse.
   * **Tasta 1 (`SCENE_LEVEL`):** Intensitate mică, fixată la 20%.
   * **Tasta 2 (`SCENE_LEVEL`):** Intensitate medie, fixată la 55%.
   * **Tasta 3 (`SCENE_LEVEL`):** Intensitate maximă, fixată la 90%.
   * **Tasta 4 (`SCENE_RELAX`):** Efect de *breathing* (creștere și descreștere graduală a intensității), iar LED-ul RGB își schimbă culoarea atunci când se atinge intensitatea minimă sau maximă.
   * **Tasta 5 (`SCENE_PARTY`):** LED-ul RGB își schimbă culoarea periodic (după un număr fix de iterații), în timp ce LED-urile standard sunt stinse.
   * **Tasta 7 (`SCENE_LED_BREATH`):** Cele 4 LED-uri normale (LD0-LD3) prezintă un efect continuu de *breathing* (duty-ul variază în buclă între 0 și maxim).
   * **Tasta F (`SCENE_AUTO_OFF`):** Indiferent de starea curentă, LED-urile se sting treptat (duty-ul scade gradual) până la 0 pe parcursul a 10 secunde.

## 📂 Structura Repository-ului
* `/Hardware_VHDL` - Conține sursa `pwm.vhd`, constrângerile piniilor `.xdc` și scriptul `.tcl` care generează Block Design-ul în Vivado.
* `/Software_C` - Conține aplicația bare-metal scrisă în C (`mainProject.c`) rulată din mediul Vitis.
* `/Docs` - Documentația completă a proiectului.