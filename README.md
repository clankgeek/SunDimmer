# SunDimmer

Routeur solaire sur Arduino Nano. Il envoie le surplus de production des panneaux vers le chauffe-eau au lieu de l'injecter sur le réseau. Le chauffe-eau sert ainsi de batterie thermique.

Le montage mesure en continu le flux avec le compteur Enedis et la consommation du chauffe-eau. Il règle un variateur (dimmer) pour que la maison injecte le moins possible.

## Fonctionnement

À chaque mesure, le routeur calcule :

```
puissance chauffe-eau = conso actuelle chauffe-eau - flux Enedis - 100 W
```

- Flux Enedis négatif = injection vers le réseau, donc surplus disponible.
- Une marge de 100 W évite d'importer à cause des erreurs de mesure.
- La puissance est convertie en pourcentage du dimmer à partir de `Pmax`, la puissance maximale du chauffe-eau mesurée pendant la calibration.
- Sous 10 % le dimmer est coupé, et il est plafonné à 90 % de `Pmax`.
- Si la lecture du compteur JSY échoue, le dimmer est coupé.

## Matériel

| Composant | Rôle |
|---|---|
| Arduino Nano (ATmega328) | Microcontrôleur |
| JSY-MK-194 | Wattmètre 2 voies : voie 1 = chauffe-eau, voie 2 = arrivée Enedis |
| Dimmer RobotDyn (compatible RBDdimmer) | Variateur à détection de passage à zéro |
| LCD 16x2 I2C | Affichage (adresse `0x3F`, certains écrans utilisent `0x27`) |
| RTC DS1307 | Horloge |
| 2 boutons poussoirs | Configuration |
| 1 interrupteur | Mode forcé |

## Câblage

| Broche Nano | Raccordement |
|---|---|
| D0 / D1 (RX/TX) | JSY-MK-194 (série 9600 bauds) |
| D2 | Dimmer, passage à zéro (Z-C) |
| D5 | Dimmer, commande (PSM) |
| D6 | Interrupteur mode forcé, vers GND |
| D7 | Bouton 1, vers GND |
| D8 | Bouton 2, vers GND |
| A4 / A5 (SDA/SCL) | LCD et RTC (bus I2C partagé) |

Les boutons et l'interrupteur utilisent la résistance de pull-up interne. Ils se raccordent donc entre la broche et GND, sans résistance externe.

Le JSY-MK-194 est réglé d'usine à 4800 bauds. Passez-le à 9600 avec `jsy.changeBitrate(9600)` avant la première utilisation.

Le JSY partage le port série avec l'USB. Débranchez-le pendant le téléversement du firmware.

> **Attention : 230 V.** Le dimmer et le JSY sont raccordés au secteur. Coupez le disjoncteur avant toute intervention, et protégez le circuit du chauffe-eau par un disjoncteur adapté. Le dimmer doit être dimensionné pour la puissance du chauffe-eau et correctement refroidi.

## Compilation et téléversement

Le projet utilise [PlatformIO](https://platformio.org/). Les bibliothèques sont déclarées dans `platformio.ini` et téléchargées automatiquement.

```sh
pio run                 # compiler
pio run -t upload       # téléverser (JSY débranché)
pio device monitor      # moniteur série
```

## Utilisation

### Premier démarrage

À la première mise sous tension, l'EEPROM est vierge et le montage lance la configuration :

1. **Réglage de l'heure.** Bouton 1 = `+`, bouton 2 = `ok`, pour l'heure puis les minutes.
2. **Calibration.** Le dimmer passe à 100 % et le montage relève la puissance maximale du chauffe-eau sur 128 mesures. Cette valeur est enregistrée en EEPROM.

L'eau doit être froide pendant la calibration. Si le thermostat a coupé, le chauffe-eau ne consomme rien : après environ 60 s sans mesure valable, l'écran affiche `Calibration KO`. Rien n'est enregistré, le routage reste désactivé, et il faudra redémarrer avec de l'eau froide.

### Fonctionnement normal

L'écran affiche :

```
14:32, EDF -850w
Out 1200w
```

- Ligne 1 : heure et flux Enedis (négatif = injection).
- Ligne 2 : puissance envoyée au chauffe-eau.

### Mode forcé

Interrupteur D6 fermé : le chauffe-eau est alimenté à 100 % quel que soit le surplus, et l'écran affiche `Mode manuel ON`. Ouvrez l'interrupteur pour revenir au routage automatique.

### Reconfiguration

Maintenez le **bouton 1** pendant le démarrage, puis relâchez-le :

1. `Calibrer Pmax ?` : bouton 1 = oui, bouton 2 = non.
2. Réglage de l'heure.

### Mode wattmètre simple

Maintenez le **bouton 2** pendant le démarrage. L'écran affiche les deux puissances mesurées (`w1` = chauffe-eau, `w2` = Enedis), sans routage. Redémarrez pour quitter ce mode.

## Réglages dans le code

| Constante / valeur | Emplacement | Défaut |
|---|---|---|
| `SERIAL_BITRATE` | `src/main.cpp` | 9600 |
| `LCD_ADDRESS` | `src/main.cpp` | `0x3F` |
| Marge anti-import | `router()` | 100 W |
| Seuil mini / plafond | `setDimmer()` | 10 % / 90 % de `Pmax` |
| Adresse EEPROM de `Pmax` | `EEPROM_ADDR` | 14 |
