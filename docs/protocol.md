# Protocole du moteur de jeu — version 1

Ce document décrit comment le moteur de jeu (`./server`) dialogue avec ce qui
le pilote. Le moteur arbitre une seule partie : il pose des questions pour
les décisions des mages et annonce tout ce qui se passe. Il ne prend aucune
décision à la place des mages.

**Le moteur ne cache rien.** Toutes ses sorties sont complètes : cartes face
cachée, mains, contrats, ordre du Futur, graine. C'est la couche serveur
écrite en Godot qui décide ce que chaque joueur humain a le droit de voir.
Pour l'aider, la colonne « Secret » des tableaux indique ce qui est caché
dans le jeu physique. C'est une documentation : rien de tel n'apparaît dans
les messages.

Les bots de test (section 8) parlent directement au moteur. Ils reçoivent
donc tout, et c'est à leur code de ne lire que ce qu'un mage peut savoir.

## 1. Transport

- **Format** : JSON Lines. Chaque message est un objet JSON sur une seule
  ligne, encodé en UTF-8 et terminé par `\n`.
- **Moteur → pilote** : la sortie standard du moteur. Elle ne contient que
  des messages du protocole.
- **Pilote → moteur** : l'entrée standard du moteur. Elle ne contient que les
  réponses aux questions.
- **Journal lisible** : la sortie d'erreur du moteur, qui n'est pas
  interprétée.
- **Un seul canal pour toute la partie.** Chaque question indique le siège
  qui doit décider (`seat`), et chaque réponse rappelle ce siège.

### Ordre des messages

- Le moteur n'attend jamais plus d'une réponse à la fois. Après une
  question, il n'écrit plus rien tant qu'il n'a pas reçu de réponse valide,
  ou décidé d'arrêter la partie.
- Si l'entrée standard est fermée pendant une attente, le moteur envoie
  `game_aborted` et s'arrête avec le code de sortie 2.

## 2. Identifiants et représentation des cartes

- **Sièges** : entiers de `0` à `N-1`, dans l'ordre du tour. Le siège 0
  commence.
- **Factions** : `"RED"`, `"YELLOW"`, `"BLUE"`. `"ANY"` n'apparaît que sur une
  carte, jamais dans un choix.
- **Événements** : chaque carte événement a un `id` unique et stable pour
  toute la partie. Les branches d'une propagande ET / OU n'ont pas d'`id`.
- **Positions dans les zones** : index à partir de 0.
  - Présent : de la première carte révélée à la dernière.
  - Futur : `0` est le dessus de la pile.
  - Passé et Oubli : dans l'ordre d'arrivée.
  - Main, cartes manigancées, Cosmos : dans l'ordre de la zone.

### Événement

Les clés reprennent celles des fichiers `events/*.json` :

```json
{"id":17,"kind":"PROPAGANDA","faction":"RED","value":3,"base_value":2,"stealth":4}
{"id":52,"kind":"AND","left":{"faction":"ANY","value":1,"base_value":1,"stealth":1},
                      "right":{"faction":"RED","value":2,"base_value":2,"stealth":2}}
{"id":3,"kind":"ELECTION","stealth":3}
```

- `kind` vaut `PROPAGANDA`, `AND`, `OR`, `ELECTION`, `COLLAPSE`, `EXPLOSION`
  ou `MISTAKE`.
- `value` est la valeur actuelle, modifications comprises, et `base_value` la
  valeur imprimée. Les deux sont présentes sur toute propagande.

### Carte grimoire

Une carte grimoire est représentée par son nom, comme dans `decks/*.json` :
`"ETHER"`, `"EUPHORIA"`, etc.

### Contrat

```json
{"name":"RED","factions":["RED"],"rank":1,"points":1}
```

## 3. Questions et réponses

### Question (moteur → pilote)

```json
{"type":"ask","id":42,"seat":1,"question":"propaganda_target",
 "context":{"spell":"EUPHORIA","modification":2},
 "options":[{"present":0},{"present":3}],
 "state":{ ... }}
```

- `id` : entier strictement croissant sur toute la partie.
- `seat` : le mage qui doit décider.
- `question` : un des noms du catalogue ci-dessous.
- `context` : ce qui motive la question.
- `options` : liste **non vide** des choix permis. Le moteur pose la
  question **même quand il n'y a qu'une option**, parce que les stratégies
  de la simulation tirent au hasard même dans ce cas : c'est ce qui garantit
  qu'une partie jouée via le moteur est identique à la même partie jouée par
  `./simulation`. La couche Godot peut répondre seule à ces questions sans
  déranger le joueur humain.
- Exceptions : `next_shenanigan` n'est posée que s'il y a au moins deux
  cartes manigancées. `propaganda_target` et `opponent_target` ne sont pas
  posées quand il n'y a aucune cible : le sort échoue (`spell_failed`).
  `ether_leak_target` est posée pour chacun des 1 + éther dépensé éthers
  que le sort peut défausser, jusqu'à un `"stop"`, même quand `"stop"` est
  la seule option restante.
- `state` : l'état complet de la partie (section 5). On peut donc répondre
  sans avoir suivi les messages précédents.

### Réponse (pilote → moteur)

```json
{"seat":1,"id":42,"choice":1}
```

`choice` est l'**index de l'option retenue** dans `options`. Ici, c'est la
propagande en position 3 du Présent.

### Réponse refusée

Le moteur refuse une réponse dans ces cas :
- ligne qui n'est pas du JSON ;
- mauvais `seat` ou mauvais `id` ;
- `choice` qui n'est pas un index valide.

Chaque question a droit à **2 essais** :

1. Si la première réponse est invalide, le moteur envoie un message
   d'erreur, puis **renvoie la même question**, avec le même `id` :

   ```json
   {"type":"error","id":42,"seat":1,"reason":"choice must be between 0 and 1"}
   ```

2. Si la deuxième réponse est encore invalide, le moteur envoie `error` puis
   `game_aborted`, et s'arrête avec le code de sortie 3.

Il n'y a pas de limite de temps : le moteur attend indéfiniment. Les délais,
s'il en faut, sont gérés par la couche Godot.

### Catalogue des questions

| `question`          | Quand                                             | `context`                      | `options`                                                |
|---------------------|---------------------------------------------------|--------------------------------|----------------------------------------------------------|
| `reveal_more`       | phase de Révélation, au mage dont c'est le tour   | —                              | `["reveal","stop"]` (posée même si la furtivité est à 0) |
| `scheme_card`       | phase de Manigances, main non vide                | —                              | `[{"hand":i,"card":"EUPHORIA"}, …]`                      |
| `next_shenanigan`   | phase de Sortilèges, plusieurs cartes manigancées | —                              | `[{"prepared":i,"card":…}, …]`                           |
| `reveal_shenanigan` | phase de Sortilèges, pour chaque carte manigancée | `{"card":…}`                   | `["reveal","bury"]` (bury : sous le grimoire)            |
| `ether_to_spend`    | un sort révélé, éther disponible                  | `{"spell":…}`                  | `[0,1,…,éther disponible]`                               |
| `propaganda_target` | Euphorie / Neurasthénie                           | `{"spell":…,"modification":m}` | `[{"present":i}, …]`                                     |
| `opponent_target`   | Trou de mémoire                                   | `{"spell":"MEMORY_LAPSE"}`     | `[{"seat":k}, …]`                                        |
| `cosmos_card`       | Renverser l'entropie, une fois par carte reprise  | `{"spell":…,"remaining":r}`    | `[{"cosmos":i,"card":…}, …]`                             |
| `ether_leak_target` | Fuite d'éther, une fois par éther visé            | `{"spell":…,"remaining":r}`    | `[{"seat":k,"ether":e}, …, "stop"]`                      |
| `divergence_branch` | résolution d'une propagande OU                    | `{"event":…,"present":i}`      | `["left","right"]`                                       |
| `any_faction`       | résolution d'une propagande ANY                   | `{"event":…,"value":v}`        | `["RED", …]` (factions permises)                         |

**Secret** : dans le jeu physique, seuls `reveal_shenanigan` et
`scheme_card` sont des choix cachés. Les autres choix se font à la vue de
tous.

## 4. Annonces (moteur → pilote)

Toutes les annonces ont la forme `{"type":…, …}` et contiennent toute
l'information, cachée ou non.

### Début et fin de partie

| `type`         | Champs                                                                                          | Secret dans le jeu physique                  |
|----------------|-------------------------------------------------------------------------------------------------|----------------------------------------------|
| `game_start`   | `protocol:1`, `seed`, `seats`, `max_stealth`, `hand_size`, `popularity` (`{"RED":0,…}`), `future` (liste complète, dessus d'abord) | `seed` et `future` : de tous            |
| `seat_setup`   | `seat`, `contract`, `deck` (nom du deck de départ), `hand`, `grimory` (ordre de pioche, dessus d'abord) | `contract`, `hand` : des autres ; `grimory` : de tous |
| `game_over`    | `scores` (par siège), `winner`, `tiebreak` (`none`/`grimory`/`draw`), `contracts`               | —                                            |
| `game_aborted` | `reason`, `seat` (siège fautif, ou `null` si l'entrée standard est fermée)                      | —                                            |
| `error`        | `id` (question concernée), `seat`, `reason`                                                     | —                                            |

### Tour

| `type`       | Champs                                                              | Secret |
|--------------|---------------------------------------------------------------------|--------|
| `turn_start` | `turn`, `current` (siège), `future_size`                            | —      |
| `turn_end`   | `outcome` (`resolved` / `stealth_rollback` / `explosion_rollback`)  | —      |

### Manigances

| `type`         | Champs                   | Secret               |
|----------------|--------------------------|----------------------|
| `card_schemed` | `seat`, `hand` (index), `card` | `card` : des autres |

### Révélation

| `type`                 | Champs                                                                                     | Secret |
|------------------------|--------------------------------------------------------------------------------------------|--------|
| `event_revealed`       | `event`, `present` (position), `stealth` (total), `future_size`                            | —      |
| `collapse`             | — (la phase de Révélation s'arrête)                                                        | —      |
| `rollback`             | `cause` (`stealth`/`explosion`), `to_oblivion` (événement ou `null`), `future` (ids des cartes remises sur le Futur, dessus d'abord) | —      |
| `shenanigans_returned` | `cards` : `[{"seat":k,"card":…}, …]` (cartes manigancées reprises en main)                 | `card` : des autres |

### Sortilèges

| `type`                | Champs                                                         | Secret                      |
|-----------------------|----------------------------------------------------------------|-----------------------------|
| `shenanigan_buried`   | `seat`, `card` (placée sous le grimoire)                       | `card` : des autres         |
| `shenanigan_revealed` | `seat`, `card`                                                 | —                           |
| `ether_stored`        | `seat`, `ether` (total en réserve)                             | —                           |
| `ether_spent`         | `seat`, `amount`                                               | —                           |
| `spell_failed`        | `seat`, `card` (aucune cible)                                  | —                           |
| `choice_cancelled`    | `immune` (siège), `by` (siège), `charges_left`                 | —                           |
| `propaganda_modified` | `present`, `modification`, `event` (après modification)        | —                           |
| `memory_lapse`        | `from`, `to`                                                   | —                           |
| `cosmos_taken`        | `seat`, `card`                                                 | —                           |
| `ether_leaked`        | `seat` (qui perd l'éther), `ether` (restant)                   | —                           |
| `immunity_gained`     | `seat`, `charges`                                              | —                           |
| `future_read`         | `seat`, `events` (dessus du Futur d'abord)                     | `events` : des autres       |
| `chapter_skipped`     | `events` (placés dans le Passé)                                | —                           |
| `forced_reveal`       | `count` (Précipitation : les `event_revealed` suivent)         | —                           |

### Résolution

| `type`           | Champs                                                                                     | Secret |
|------------------|--------------------------------------------------------------------------------------------|--------|
| `event_resolved` | `event`, `applied` (`[{"faction":"RED","value":2}, …]` après choix OU / ANY), `popularity` | —      |
| `election`       | `points` (gagnés par siège), `scores` (totaux), `popularity`                               | —      |

### Fin de tour

| `type`        | Champs                                              | Secret             |
|---------------|-----------------------------------------------------|--------------------|
| `cards_drawn` | `seat`, `cards`, `grimory_size`                     | `cards` : des autres |

## 5. L'état de la partie (`state`)

```json
{
  "turn": 7, "current": 0, "max_stealth": 21, "stealth": 9,
  "popularity": {"RED": 4, "YELLOW": 2, "BLUE": 5},
  "present": [ <événement>, … ],
  "future":  [ <événement>, … ],
  "past":    [ <événement>, … ],
  "oblivion":[ <événement>, … ],
  "cosmos":  ["EUPHORIA", "ETHER", …],
  "seats": [
    {"seat": 0, "contract": <contrat>, "hand": ["ETHER", …], "prepared": ["EUPHORIA"],
     "ether": 1, "immunity": 0, "score": 2, "grimory": ["ETHER", …],
     "known_future": [17, 52]},
    …
  ]
}
```

- `future` est dans l'ordre réel de la pile, dessus d'abord.
- `grimory` est dans l'ordre de pioche, dessus d'abord.
- `known_future` liste les `id` des cartes du Futur que ce mage a vues avec
  Lecture. Ce sont les seules cartes du Futur qu'un bot honnête peut
  utiliser.

## 6. Exemple de début de partie (3 mages)

```
→ {"type":"game_start","protocol":1,"seed":1234,"seats":3,"max_stealth":21,"hand_size":4,"popularity":{"RED":0,"YELLOW":0,"BLUE":0},"future":[…]}
→ {"type":"seat_setup","seat":0,"contract":{"name":"RED","factions":["RED"],"rank":1,"points":1},"deck":"implemented","hand":["ETHER","EUPHORIA","ETHER","MEMORY_LAPSE"],"grimory":[…]}
→ {"type":"seat_setup","seat":1, …}
→ {"type":"seat_setup","seat":2, …}
→ {"type":"turn_start","turn":0,"current":0,"future_size":128}
→ {"type":"ask","id":0,"seat":0,"question":"scheme_card","options":[{"hand":0,"card":"ETHER"}, …],"state":{…}}
← {"seat":0,"id":0,"choice":1}
→ {"type":"card_schemed","seat":0,"hand":1,"card":"EUPHORIA"}
→ {"type":"ask","id":1,"seat":1,"question":"scheme_card", …}
…
→ {"type":"ask","id":3,"seat":0,"question":"reveal_more","options":["reveal","stop"],"state":{…}}
← {"seat":0,"id":3,"choice":0}
→ {"type":"event_revealed","event":{"id":88,"kind":"PROPAGANDA","faction":"BLUE","value":2,"base_value":2,"stealth":4},"present":0,"stealth":4,"future_size":127}
```

## 7. Lancement

```
./server [--players N] [--deck <file.json>]... [--contracts <file.json>]
         [--events <file.json>] [--popularity <n>] [--max-stealth <n>] [--seed <n>]
```

La partie commence dès le lancement. Elle se termine quand le Futur est
vide : le moteur envoie alors `game_over` et s'arrête avec le code de
sortie 0.

## 8. Bots de test

Le programme `./bot --strategy random|smart --seat k` lit les messages du
moteur et répond aux questions de son siège. Il prend ses décisions à partir
du `state` de chaque question, mais ne lit que ce que son mage peut savoir :
- les zones face visible ;
- sa propre main, ses cartes manigancées et son contrat ;
- les tailles des mains et des grimoires adverses ;
- les cartes du Futur listées dans son `known_future`.

Un script d'aiguillage lance le moteur et un bot par siège, puis transmet à
chaque bot les messages du moteur et au moteur les réponses des bots. À
graine égale, une partie jouée ainsi doit donner exactement le même résultat
qu'une partie jouée par `./simulation`.
