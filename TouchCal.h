/*
TouchCal Mini-biblioteca para pantallas táctiles SPI (XPT2046_Touchscreen) - by GuerraTron24
---------------------

 Utilidades para la TouchScreen como la calibración de la pantalla, comprobar toque válido en función de la presión, 
 figuras lissajous para comprobar la simetría de la screen, e incluso "Modo-Oscuro" sin display ..
 Basado en los ejemplos de la librería "TFT_eSPI" de bodmer; utiliza "XPT2046_Touchscreen" de Paul Stoffregen.
 [QUE PEDAZO UTILIDADES]. Aunque es más visual y estético la utilización de un display, en realidad existen proyectos 
 donde no son imprescindibles. Por eso esta biblioteca NO tiene DEPENDENCIA DIRECTA con "TFT_eSPI", se puede trabajar 
 sin esa "maravillosa utilidad" (pero sí con "XPT2046_Touchscreen"). 

 Esta mini-librería surge de la necesidad de obtener parámetros de configuración de la pantalla de toque a raiz de 
 algunos experimentos en los que me fallaba la función original TFT_eSPI::calibrateTouch(..), se quedaba colgado el 
 dispositivo con ciertas configuraciones de pantalla, pines y SPI.

 La función de calibración la he puesto más visual aunque se incremente el costo computacional, ya que se supone que 
 no es una librería de usuario a la usanza, más bien una herramienta para el programador que cuando termine la depuración 
 seguramente la inhiba.

 La función principal sería "calibration(..)" que admite como parámetros (PUNTEROS) la touchscreen y la screen (si está disponible), 
 además de colores y tamaños para las flechas de las esquinas (al igual que "TFT_eSPI::calibrateTouch(..)"). 
 La función realiza las mismas comprobaciones que la original más algunos añadidos extra, pero se basa en el objeto "*touchscreen" 
 pasado como parámetro y que se supone que ya se encuentra iniciado para detectar los toques, en vez del objeto por defecto que 
 ataca la librería original.  

 > Así, con esta librería NO IMPORTA en qué pines se conecte la pantalla táctil, ni que tipo de SPI utilice, ya que esto viene 
 > establecido con aterioridad a la utilización de esta librería. 

 El parámetro de la screen (TFT_eSPI *tft) se utiliza para dar visualización a las indicaciones y los datos leídos, aunque no 
 es imprescindible ya que esos mismos datos se están mandando en paralelo al monitor serie (qué curiosa expresion, "paralelo-serie"!), 
 he incluso pueden optenerse en tiempo de ejecución a través del array global "TC_PARS[5]".  

 Lo que sí necesita son las dimensiones de la pantalla: "TFT_HEIGHT" y "TFT_WIDTH", y los colores predefinidos, además de la rotación 
 "TC_ROTATION"; en función de esta rotación los valores de "width" y "height" se invertirán.
 
 FULL-MODE:
 Utilizar por ejemplo: ``` tc::calibration(&ts, &tft); ```

 Un uso normal sería su utilización en el "setup" después de haber inicializado el resto de librerías "Serial, TFT_eSPI, 
 XPT2046_Touchscreen" necesarias; o por ejemplo también en un "handler" de algún botón de acción.

 DARK-MODE:
 Oro uso simplificado pordría ser utilizando únicamente la "touchscreen" (sin screen) para lo cual habría que definir 
 previamente algunas definiciones utilizadas por la librería "TFT_eSPI" (dimensiones y colores), entonces se reduciría a:
 ``` tc::calibration(&ts); ```
 
 PIN-MODE:
 Otro modo de trabajo (EXTREMO), sin display ni puerto Serial, podría implementarse informando a través de un LED conectado al pin definido en 
 "PIN_PRINT", por defecto el 26 en ESP32. Esta forma es muy arcaica pero serviría para identificar los toques en las esquinas en función 
 del parpadeo del LED (corner1 = 1, corner2 = 2, ..) y sólo necesitaríamos la parte táctil de la pantalla. A partir de aquí ya estaría 
 calibrada la táctil, pero claro, cada vez que iniciemos habría que recalibrarla de nuevo.

 ATENCIÓN: Definir la rotación a utilizar con la screen y la touch en "TC_ROTATION" (del 0 al 3)

----
 SE HA IMPLEMENTADO TODO EN UN ÚNICO ARCHIVO DE CABECERA ".H" POR NUMEROSOS PROBLEMAS EN EL "LINKER" AL SEPARARLO EN ARCHIVOS ".cpp"
----

 by GuerraTron24 - GPL v3 - <dinertron@gmail.com>
*/
#ifndef _TC1_H
#define _TC1_H

#include <Arduino.h>
/* la siguiente definición es sólo para que el IDE no me dé problemas, podría eliminarse porque en 
 * realidad es una tontería: "Sólo incluye la librería si realmente ya se encuentra definida"
//#ifdef _TFT_eSPIH_
#ifdef _TFT_eSPIH_
#include <TFT_eSPI.h>
#endif */
/* esta otra SI es obligatoria (DEPENDENCIA) */
#ifndef XPT2046_Touchscreen
//#error "TouchCal NECESITA XPT2046_Touchscreen, POR FAVOR INCLUIR LA BIBLIOTECA PRIMERO."
#include <XPT2046_Touchscreen.h>
#endif

/* Estas variables existirán a través de la biblioteca "TFT_eSPI". Pero por si acaso se dan valores por 
 * defecto para una pantalla SPI de 240X320 */
#ifndef TFT_WIDTH
#define TFT_WIDTH 240
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT 320
#endif

/* PRECALCULATE. Modificar con los reales obtenidos tras la prueba.  
 * Irán a parar de forma dinámica al array "TC_PARS" y en sucesivos 
 * arranques se encontrarán disponibles sin necesidad de calibración. */
/* ---------------- */
#define TC_PRE_X_MIN 421
#define TC_PRE_X_MAX 3807
#define TC_PRE_Y_MIN 205
#define TC_PRE_Y_MAX 3748
#define TC_PRE_Z 600
/* ---------------- */

/* Activar si se desea señal visual en un pin, por ejemplo si no se tiene "TFT_eSPI" ni "Serial". */
#define PIN_PRINT 26


/* ========== */
/* DEFINITION */
/* ========== */

/** Name-Space Touch Calibration: 
  * Espacio de nombres para no contaminar el scope (TC = TouchCal = TouchCalibration). 
  * Utilizar todas las funciones-variables anteponiendo "tc::" */
namespace tc{
  /*STATIC*/
  /* umbral de presión y margen de error */
  const uint16_t _threshold = TC_PRE_Z;
  const uint16_t _xyErr = 50;
  /*DYNAMIC*/
  uint8_t TC_ROTATION=0;
  /* estas dimensiones pueden invertirse en tiempo de ejecución en función de la rotación */
  uint16_t currW = TFT_WIDTH - 1;
  uint16_t currH = TFT_HEIGHT - 1;
  /* Save the touchscreen calibration data : {xMin, xMax, yMin, yMax, (rotate | (invert_x <<1) | (invert_y <<2))} */
  uint16_t TC_PARS[5] = {TC_PRE_X_MIN, TC_PRE_X_MAX, TC_PRE_Y_MIN, TC_PRE_Y_MAX, TC_PRE_Z};
  TS_Point TC_POINT_DEFAULT = TS_Point(0, 0, 0);
  /* variables temporales que terminarán en "TC_PARS" */
  uint16_t cal_x0, cal_x1, cal_y0, cal_y1;
  /* TODO: parece que no siempre lo detecta correctamente */
  bool cal_rotate, cal_invert_x, cal_invert_y;

  /** Utiliza la función arduino "map" para mapear valores puros entregados procedentes de los leídos en bruto del 
    * toque de la táctil, convertidos en coordenadas gráficas de la pantalla.
    * Los valores resultantes gráficos se almacenarán en los parámetros x é y entregados.
    * Se tienen en cuenta las dimensiones gráficas almacenadas en las globales "currW" y "currH" */
  void _map(uint16_t *rawX, uint16_t *rawY, uint16_t *x, uint16_t *y);

  /** Obtiene las coordenadas reales del toque (x, y, z) rellenadas en el punto entregado.  
    * Necesita un punto en el que devolver los resultados y POR SUPUESTO la touchscreen definida 
    * externamente con la que trabajar (tiene que estar definida e iniciada en algún SPI válido).
    */
  void _getTouchRaw(XPT2046_Touchscreen *ts, TS_Point *p);

  /** Comprueba (y rellena el punto) si un toque es válido en cuanto no tenga unidades z, x é y negativas, 
    * y la z tenga un valor por encima del umbral de presión.  
    * El punto sólo es necesario si se desean obtener resultados de coordenadas, sinó, se utiliza uno temporal.  
    * Un sólo paso. */
  bool _validTouchPoint(XPT2046_Touchscreen *ts, TS_Point *p  = &TC_POINT_DEFAULT, const uint16_t threshold = _threshold);

  /** Comprueba si un toque es válido en cuanto no tenga unidades z, x é y negativas, y la z tenga un valor por 
    * encima del umbral de presión. Admite un umbral de presión y un margen de error entre lecturas.  
    * Además realiza la comprobación en dos pasos para mayor exactitud (puede demorar entre 7 y 10 msg).  
    * También rellena las coordenadas x é y. 
    * Trabaja con "_validTouchPoint(..)" pero realiza más comprobaciones y almacena en x e y en vez de en point. 
    * Las coordenadas no están mapeadas. */
  bool _validTouch(XPT2046_Touchscreen *ts, uint16_t *x, uint16_t *y, const uint16_t threshold = _threshold, const uint16_t xyErr = _xyErr);

  /***************************************************************************************
  ** Function name:           setTouch
  ** Description:             imports calibration parameters for touchscreen. 
  ***************************************************************************************/
  void _setTouch(uint16_t *parameters);

  /* ======= PUBLIC ======= */

  /** Comprueba si hay toque y rellena en el punto las coordenadas Reales (Display) de ese toque. 
    * También obtiene la presión en "z" */
  bool isTouch(XPT2046_Touchscreen *ts, TS_Point *p, const uint16_t threshold = _threshold);
  /** Comprueba si hay toque y rellena las coordenadas Reales (Display) de ese toque. (NO Z)*/
  bool isTouch(XPT2046_Touchscreen *ts, uint16_t *x, uint16_t *y, const uint16_t threshold = _threshold, const uint16_t xyErr = _xyErr);

  /** Método de conveniencia, por compatibilidad con otras librerías. 
    * intercambia width y height en base a la rotación.  
    * Valores del 0 al 3 */
  void setRotation(uint8_t rotation);

    /** Hace parpadear un led como información de la esquina tocada. */
  void printToPin(uint8_t corner = 0, bool enabled = false);

#ifdef _TFT_eSPIH_
  /***************************************************************************************
  ** Function name:           calibration Touch calibration
  ** Description:             generates calibration parameters for touchscreen.
  * @param ts {XPT2046_Touchscreen*} La touchscreen (puntero) de la que obtener las coordenadas.
  * @param tft {TFT_eSPI*} La tft (puntero) en la que dibujar. 
  ***************************************************************************************/
  void calibration(XPT2046_Touchscreen *ts, TFT_eSPI *tft, const uint32_t color_fg = TFT_MAGENTA, const uint32_t color_bg = TFT_BLACK, const uint8_t size = 10);
#else
  /***************************************************************************************
  ** Function name:           calibration Touch calibration
  ** Description:             generates calibration parameters for touchscreen.
  * @param ts {XPT2046_Touchscreen*} La touchscreen (puntero) de la que obtener las coordenadas.
  ***************************************************************************************/
  void calibration(XPT2046_Touchscreen *ts);
#endif /* TFT_eSPI */

#ifdef _TFT_eSPIH_
  /** Pequeña utilidad para visualizar curvas de Lissajous en función de las coordenadas de toque.  
  * Puede obtenerse una ligera idea de la simetría de la pantalla a través de la visualización de las imágenes. */
  void drawLissajous(XPT2046_Touchscreen *ts, TFT_eSPI *tft) ;
#endif /* TFT_eSPI */

}  /* END: "TC" NAMESPACE */

/* ============== */
/* IMPLEMENTATION */
/* ============== */

void tc::_map(uint16_t *rawX, uint16_t *rawY, uint16_t *x, uint16_t *y){
    // the points with will map function to the correct width and height
    *x = map(*rawX, tc::TC_PARS[0], tc::TC_PARS[1], 1, tc::currW);
    *y = map(*rawY, tc::TC_PARS[2], tc::TC_PARS[3], 1, tc::currH);
    /* no sé porqué tienen que salirse los valores del map anterior, brruzcz!!*/
    *x = (*x<1) ? 1 : *x;
    //*x = (*x>tc::currW) ? tc::currW : *x;
    *y = (*y<1) ? 1 : *y;
    //*x = (*y>tc::currH) ? tc::currH : *y;
}

void tc::_getTouchRaw(XPT2046_Touchscreen *ts, TS_Point *p){
  if( /*(*ts != NULL) &&ts->tirqTouched() &&*/ ts->touched()) { // || Podría modificarse si la touchscreen tiene habilitada la línea de "irq" 
    *p = ts->getPoint();// Get Touchscreen points
    //protección frente a negativos
    p->x = (p->x < 1) ? 1 : p->x;
    p->y = (p->y < 1) ? 1 : p->y;
    p->z = (p->z < 1) ? 1 : p->z;
    //TS_Point pp = ts->getPoint();
    //Serial.print("-x:"); Serial.print((unsigned long)pp.x); Serial.print(", -y:"); Serial.println((unsigned long)pp.y);
    delay(1);
  }
}

bool tc::_validTouchPoint(XPT2046_Touchscreen *ts, TS_Point *p, const uint16_t threshold){
  tc::_getTouchRaw(ts, p);
  return (p->x >= 0) && (p->y >= 0) && (p->z >= 0) && (p->z > threshold);
}

bool tc::_validTouch(XPT2046_Touchscreen *ts, uint16_t *x, uint16_t *y, const uint16_t threshold, const uint16_t xyErr){
  TS_Point p1 = TS_Point(0, 0, 0); //TS_Point *pp1 = &p1;
  if(!tc::_validTouchPoint(ts, &p1, threshold)){ return false; }
  delay(5);
  
  TS_Point p2 = TS_Point(0, 0, 0); //TS_Point *pp2 = &p2;
  if(!tc::_validTouchPoint(ts, &p2, threshold)){ return false; }

  if (abs(p1.x - p2.x) > xyErr){ return false; }
  if (abs(p1.y - p2.y) > xyErr){ return false; }

  *x = (p1.x + p2.x) / 2;
  *y = (p1.y + p2.y) / 2;
  //*x = (uint16_t)((p1.x + p2.x) / 2);
  //*y = (uint16_t)((p1.y + p2.y) / 2);

  //free(p1);
  //free(p2);
  //delete &p1;
  //delete &p2;
  //Serial.print("-x:"); Serial.print((unsigned long)x); Serial.print(", -y:"); Serial.println((unsigned long)y);
  /*
      Serial.println("===========");
      Serial.print("-x1:"); Serial.print((long)p1.x); Serial.print(", -y1:"); Serial.println((long)p1.y);
      Serial.print("-x1:"); Serial.print((long)pp1->x); Serial.print(", -y1:"); Serial.println((long)pp1->y);
      Serial.println("-----------");
      Serial.print("-x2:"); Serial.print((long)p2.x); Serial.print(", -y2:"); Serial.println((long)p2.y);
      Serial.print("-x2:"); Serial.print((long)pp2->x); Serial.print(", -y2:"); Serial.println((long)pp2->y);
      Serial.println("===========");
    */
  return true;
}

void tc::_setTouch(uint16_t *parameters){
  tc::cal_x0 = parameters[0];
  tc::cal_x1 = parameters[1];
  tc::cal_y0 = parameters[2];
  tc::cal_y1 = parameters[3];

  if(tc::cal_x0 == 0) tc::cal_x0 = 1;
  if(tc::cal_x1 == 0) tc::cal_x1 = 1;
  if(tc::cal_y0 == 0) tc::cal_y0 = 1;
  if(tc::cal_y1 == 0) tc::cal_y1 = 1;

  tc::cal_rotate = parameters[4] & 0x01;
  tc::cal_invert_x = parameters[4] & 0x02;
  tc::cal_invert_y = parameters[4] & 0x04;
}

bool tc::isTouch(XPT2046_Touchscreen *ts, TS_Point *p, const uint16_t threshold){
    TS_Point _p = TS_Point();
    bool result = tc::_validTouchPoint(ts, &_p, threshold);
    tc::_map((uint16_t*)&_p.x, (uint16_t*)&_p.y, (uint16_t*)&p->x, (uint16_t*)&p->y);
    return result;
}
bool tc::isTouch(XPT2046_Touchscreen *ts, uint16_t *x, uint16_t *y, const uint16_t threshold, const uint16_t xyErr){
    uint16_t x_tmp, y_tmp;
    bool result = tc::_validTouch(ts, &x_tmp, &y_tmp, threshold, xyErr);
    tc::_map(&x_tmp, &y_tmp, x, y);
    return result;
}

void tc::setRotation(uint8_t rotation){
  tc::TC_ROTATION = rotation;
  tc::currW = TFT_WIDTH - 1;
  tc::currH = TFT_HEIGHT - 1;
  if(rotation == 1 || rotation == 3){
    tc::currW = TFT_HEIGHT - 1;
    tc::currH = TFT_WIDTH - 1;
  }
}

void tc::printToPin(uint8_t corner, bool enabled){
#ifdef PIN_PRINT
  //Serial.print(corner);
  pinMode(PIN_PRINT, OUTPUT);
  delay(100);
  for(int i = 0; i<corner; i++){
    digitalWrite(PIN_PRINT, HIGH);
    delay(200);
    digitalWrite(PIN_PRINT, LOW);
    delay(100);
  }
  if(enabled){ digitalWrite(PIN_PRINT, HIGH); }
#endif /* Pin-Print */
}

#ifdef _TFT_eSPIH_
void tc::calibration(XPT2046_Touchscreen *ts, TFT_eSPI *tft, const uint32_t color_fg, const uint32_t color_bg, const uint8_t size){
#else
void tc::calibration(XPT2046_Touchscreen *ts){
#endif /* TFT_eSPI */
  //uint16_t tc::currH = tft->height() - 1, tc::currW = tft->width() - 1;
  Serial.println("Touch-Calibrate..");
  tc::setRotation(tc::TC_ROTATION); /* intercambia width y height en base a la rotación. */
  Serial.print("rotation: "); Serial.print(tc::TC_ROTATION); 
  Serial.print(", width: "); Serial.print(tc::currW); Serial.print(", height: "); Serial.println(tc::currH);
  uint16_t values[] = {0,0,0,0,0,0,0,0};
  uint16_t x_tmp=0, y_tmp=0;

#ifdef _TFT_eSPIH_
  tft->setCursor(0, 0);
  // Clear the screen
  tft->fillScreen(TFT_BLACK);
  tft->setTextFont(2);
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawCentreString("Touch screen to test!", tc::currW/2, tc::currH/2, 2);
#endif /* TFT_eSPI */
  uint8_t i;
  for(i = 0; i<4; i++){
#ifdef _TFT_eSPIH_
    tft->fillRect(0, 0, size+1, size+1, color_bg);
    tft->fillRect(0, tc::currH-size-1, size+1, size+1, color_bg);
    tft->fillRect(tc::currW-size-1, 0, size+1, size+1, color_bg);
    tft->fillRect(tc::currW-size-1, tc::currH-size-1, size+1, size+1, color_bg);

    //if (i == 5) break; // used to clear the arrows
    
    switch (i) {
      case 0: // up left
        tft->drawLine(0, 0, 0, size, color_fg);
        tft->drawLine(0, 0, size, 0, color_fg);
        tft->drawLine(0, 0, size , size, color_fg);
        break;
      case 1: // bot left
        tft->drawLine(0, tc::currH-size-1, 0, tc::currH-1, color_fg);
        tft->drawLine(0, tc::currH-1, size, tc::currH-1, color_fg);
        tft->drawLine(size, tc::currH-size-1, 0, tc::currH-1 , color_fg);
        break;
      case 2: // up right
        tft->drawLine(tc::currW-size-1, 0, tc::currW-1, 0, color_fg);
        tft->drawLine(tc::currW-size-1, size, tc::currW-1, 0, color_fg);
        tft->drawLine(tc::currW-1, size, tc::currW-1, 0, color_fg);
        break;
      case 3: // bot right
        tft->drawLine(tc::currW-size-1, tc::currH-size-1, tc::currW-1, tc::currH-1, color_fg);
        tft->drawLine(tc::currW-1, tc::currH-1-size, tc::currW-1, tc::currH-1, color_fg);
        tft->drawLine(tc::currW-1-size, tc::currH-1, tc::currW-1, tc::currH-1, color_fg);
        break;
      }
#endif /* TFT_eSPI */
    // user has to get the chance to release
    if(i>0) delay(1000);

    /* Realiza 8 lecturas para obtener una media */
    uint8_t j;
    for(j= 0; j<8; j++){
      // Use a lower detect threshold as corners tend to be less sensitive
      //bool tc::_validTouch(XPT2046_Touchscreen *ts, uint16_t *x, uint16_t *y, const uint16_t threshold = tc::_threshold, const uint16_t xyErr = tc::_xyErr)
      while(!tc::_validTouch(ts, &x_tmp, &y_tmp, tc::_threshold/2));
      values[i*2  ] += x_tmp;
      values[i*2+1] += y_tmp;
      //values[i]   += x_tmp;
      //values[i+1] += y_tmp;
    }
    values[i*2  ] /= 8;
    values[i*2+1] /= 8;
    //values[i]   /= 8;
    //values[i+1] /= 8;
    
    //Aproximation's Visual Circle
    uint16_t x = 0, y = 0;
    tc::_map(&values[i*2  ], &values[i*2+1], &x, &y);
    //tc::_map(&values[i], &values[i+1], &x, &y);
#ifdef _TFT_eSPIH_
    tft->drawCircle(x, y, size*4, TFT_YELLOW);
    
    //tft->setTextFont(2);
    //tft->setTextSize(1);
    //tft->setCursor(x, y);
    tft->setTextSize(1);
    /*tft->setTextDatum(TC_DATUM); // Set datum to bottom centre
    tft->drawString("[xox]", x, y, 4);
    drawCross(tft, x, y, TFT_GREEN);*/
    //tft->drawCentreString(str, tft->width()/2, tft->height()/2, 2);
    char str[50] = "";
    snprintf(str, 50, "%d. REAL: x = %d, y = %d", i+1, x, y);
    tft->setTextColor(TFT_ORANGE, TFT_BLACK);
    tft->println();
    tft->print(str);
    snprintf(str, 50, "%d. RAW : x = %d, y = %d", i+1, values[i*2  ], values[i*2+1]);
    //snprintf(str, 50, " RAW: x = %d, y = %d", values[i], values[i+1]);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->print(str);
    str[0]='0';
#else
  if(Serial){
    char str[50] = "";
    snprintf(str, 50, " *corner %d -> REAL: x = %d, y = %d", i+1, x, y);
    Serial.println();
    Serial.println(str);
    snprintf(str, 50, " *corner %d -> RAW : x = %d, y = %d", i+1, values[i*2  ], values[i*2+1]);
    Serial.println(str);
    str[0]='0';
  }else{
#ifdef PIN_PRINT
    printToPin(i+1);
#endif /* Pin-Print */
  }
#endif /* TFT_eSPI */
  }


  // from case 0 to case 1, the y value changed. 
  // If the measured delta of the touch x axis is bigger than the delta of the y axis, the touch and TFT axes are switched.
  tc::cal_rotate = false;
  if(abs(values[0]-values[2]) > abs(values[1]-values[3])){
    tc::cal_rotate = true;
    tc::cal_x0 = (values[1] + values[3])/2; // calc min x
    tc::cal_x1 = (values[5] + values[7])/2; // calc max x
    tc::cal_y0 = (values[0] + values[4])/2; // calc min y
    tc::cal_y1 = (values[2] + values[6])/2; // calc max y
  } else {
    tc::cal_x0 = (values[0] + values[2])/2; // calc min x
    tc::cal_x1 = (values[4] + values[6])/2; // calc max x
    tc::cal_y0 = (values[1] + values[5])/2; // calc min y
    tc::cal_y1 = (values[3] + values[7])/2; // calc max y
  }
      
      /*Serial.println("===========");
      Serial.print("-x0:"); Serial.print((long)tc::cal_x0); Serial.print(", -x1:"); Serial.println((long)tc::cal_x1);
      Serial.print("-y0:"); Serial.print((long)tc::cal_y0); Serial.print(", -y1:"); Serial.println((long)tc::cal_y1);
      */

  // in addition, the touch screen axis could be in the opposite direction of the TFT axis
  tc::cal_invert_x = false;
  if(tc::cal_x0 > tc::cal_x1){
    values[0]=tc::cal_x0;
    tc::cal_x0 = tc::cal_x1;
    tc::cal_x1 = values[0];
    tc::cal_invert_x = true;
  }
  tc::cal_invert_y = false;
  if(tc::cal_y0 > tc::cal_y1){
    values[0]=tc::cal_y0;
    tc::cal_y0 = tc::cal_y1;
    tc::cal_y1 = values[0];
    tc::cal_invert_y = true;
  }
  Serial.print("rotate:"); Serial.print(tc::cal_rotate);
  Serial.print(", x-invert:"); Serial.print(tc::cal_invert_x);
  Serial.print(", y-invert:"); Serial.println(tc::cal_invert_y);
      /*Serial.println("===========");
      Serial.print("-x0:"); Serial.print((long)tc::cal_x0); Serial.print(", -x1:"); Serial.println((long)tc::cal_x1);
      Serial.print("-y0:"); Serial.print((long)tc::cal_y0); Serial.print(", -y1:"); Serial.println((long)tc::cal_y1);*/

  // pre calculate
  /*
  tc::cal_x1 -= tc::cal_x0;
  tc::cal_y1 -= tc::cal_y0;
  */

  if(tc::cal_x0 == 0) tc::cal_x0 = 1;
  if(tc::cal_x1 == 0) tc::cal_x1 = 1;
  if(tc::cal_y0 == 0) tc::cal_y0 = 1;
  if(tc::cal_y1 == 0) tc::cal_y1 = 1;

  // export parameters, if pointer valid
  tc::TC_PARS[0] = tc::cal_x0;
  tc::TC_PARS[1] = tc::cal_x1;
  tc::TC_PARS[2] = tc::cal_y0;
  tc::TC_PARS[3] = tc::cal_y1;
  tc::TC_PARS[4] = tc::cal_rotate | (tc::cal_invert_x <<1) | (tc::cal_invert_y <<2);

#ifdef _TFT_eSPIH_
  tft->setTextColor(TFT_BLUE, TFT_WHITE);
  tft->print(" tc::TC_PARS[5] = { ");
  Serial.print("  uint16_t tc::TC_PARS[5] = { ");
  for (uint8_t i = 0; i < 5; i++){
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->print(tc::TC_PARS[i]);
    Serial.print(tc::TC_PARS[i]);
    if (i < 4){
      tft->setTextColor(TFT_WHITE, TFT_BLACK);
      tft->print(", ");
      Serial.print(", ");
    }
  }
  tft->setTextColor(TFT_BLUE, TFT_WHITE);
  tft->println(" } ");
  Serial.println(" }");
  tft->setTouch(tc::TC_PARS);
  delay(200);

  tft->setTextSize(3);
  tft->setTextColor(TFT_RED, TFT_WHITE);
  tft->drawCentreString(" [X] ", tc::currW/2, tc::currH/2, 2);
  while(!tc::_validTouch(ts, &x_tmp, &y_tmp, tc::_threshold/2));
#else
  Serial.print("  uint16_t tc::TC_PARS[5] = { ");
  for (uint8_t i = 0; i < 5; i++){
    Serial.print(tc::TC_PARS[i]);
    if (i < 4){
      Serial.print(", ");
    }
  }
  Serial.println(" }");
#endif /* TFT_eSPI */
}

#ifdef _TFT_eSPIH_

void tc::drawLissajous(XPT2046_Touchscreen *ts, TFT_eSPI *tft) {
  //uint16_t tc::currH = tft->height() - 1, tc::currW = tft->width() - 1;
  tc::setRotation(tc::TC_ROTATION); /* intercambia width y height en base a la rotación. */
  uint16_t rawX=0, rawY=0;
  uint16_t x=0, y=0;
  int16_t x2=0, y2=0; // int16_t
  while(!tc::_validTouch(ts, &rawX, &rawY));
  uint16_t rX = rawX;
  uint16_t rY = rawY;
  tc::_map(&rX, &rY, &x, &y);
  uint8_t i, size=2;
  tft->fillRect(0, 0, tc::currW+1, tc::currH+1, TFT_BLACK);
  //
  char str[50] = "";
  snprintf(str, 50, " x = %d\n y = %d", x, y);//A, B
  tft->setTextSize(2);
  tft->setTextColor(TFT_ORANGE, TFT_BLACK);
  tft->setCursor(0, 0);
  tft->println();
  tft->print(str);
  
  float t = 0;
  uint8_t c = 0, len = 16;
  uint16_t colors[len] = {TFT_VIOLET,      TFT_MAGENTA,  TFT_RED,
                         TFT_ORANGE,      TFT_YELLOW,   TFT_GOLD, 
                         TFT_GREENYELLOW, TFT_GREEN,  
                         TFT_CYAN,        TFT_DARKCYAN, TFT_SKYBLUE,  TFT_BLUE, 
                         TFT_SILVER,      TFT_DARKGREY, TFT_BROWN,    TFT_MAROON  };
  for (i = 0; i < 45; i++) {
    c = i;
    if(i > len-1){ c = i/2; }
    if(i > 2*len-2){ c = i/3; }
  	//x2 = 160*sin(x*t+PI/2);
  	//y2 = 160*sin(y*t);
    x2 = tc::currW/2 * 0.9 * sin(x/10 * t);
    y2 = tc::currH/2 * 0.9 * cos(y/10 * t);
    tft->drawCircle(tc::currW/2+x2, tc::currH/2+y2, size, colors[c]);
    tft->drawCircle(tc::currW/2-x2, tc::currH/2-y2, size, colors[c]);
  	t+=0.2;
  }
}
#endif /* TFT_eSPI */

#endif