# Protocole du moteur de jeu — version 1

Ce document décrit comment le moteur de jeu (`./server`) dialogue avec ce qui
le pilote. Le moteur arbitre une seule partie : il pose des questions pour
les décisions des mages joués à distance, annonce les décisions des mages
qu'il joue lui-même (stratégies intégrées), et annonce tout ce qui se passe.

**Principe : le pilote n'a rien à déduire.** Chaque annonce dit ce qui vient
de se passer, avec les cartes concernées et leur destination. Elle porte
aussi l'état complet de la partie après ce changement (`state`, section 5).
Une interface peut donc animer à partir des annonces et se resynchroniser
sur `state` à tout moment, sans jamais rejouer les règles.

**Le moteur ne cache rien.** Toutes ses sorties sont complètes : cartes face
cachée, mains, contrats, ordre du Futur et des grimoires, graine. C'est la
couche serveur écrite en Godot qui décide ce que chaque joueur humain a le
droit de voir. Pour l'aider, la colonne « Secret » des tableaux indique ce
qui est caché dans le jeu physique. C'est une documentation : rien de tel
n'apparaît dans les messages.

## 1. Transport

- **Format** : JSON Lines. Chaque message est un objet JSON sur une seule
  ligne, encodé en UTF-8 et terminé par `\n`. Chaque ligne est écrite d'un
  coup et vidée aussitôt.
- **Moteur → pilote** : la sortie standard du moteur. Elle ne contient que
  des messages du protocole.
- **Pilote → moteur** : l'entrée standard du moteur. Elle ne contient que les
  réponses aux questions.
- **Journal lisible** : la sortie d'erreur du moteur, au format de
  `./simulation` (désactivé par `--quiet`). Il n'est pas interprété.
- **Un seul canal pour toute la partie.** Chaque question indique le siège
  qui doit décider (`seat`), et chaque réponse rappelle ce siège.

### Ordre des messages et rythme

- Le moteur ne s'arrête que pour attendre la réponse à une question. Il ne
  lit son entrée standard qu'à ce moment-là, et n'écrit plus rien tant
  qu'il n'a pas reçu de réponse valide, ou décidé d'arrêter la partie.
- Les tours joués par des stratégies intégrées s'écrivent donc d'un coup, en
  quelques millisecondes. C'est au pilote de mettre les annonces en file
  d'attente et de les jouer à son rythme (délais, animations).
- Si l'entrée standard est fermée pendant une attente, le moteur envoie
  `game_aborted` et s'arrête avec le code de sortie 2.

## 2. Identifiants et représentation des cartes

- **Sièges** : entiers de `0` à `N-1`. Le siège 0 commence.
- **Factions** : `"RED"`, `"YELLOW"`, `"BLUE"`. `"ANY"` n'apparaît que sur une
  carte, jamais dans un choix.
- **Événements** : chaque carte événement a un `id` unique et stable pour
  toute la partie. Les branches d'une propagande ET / OU n'ont pas d'`id`.
- **Positions dans les zones** : index à partir de 0.
  - Présent : de la première carte révélée à la dernière.
  - Futur et grimoires : `0` est le dessus de la pile.
  - Passé, Oubli, Cosmos : dans l'ordre d'arrivée.
  - Main et cartes manigancées : dans l'ordre de la zone. Jouer une carte de
    la main met la dernière carte à sa place : l'ordre de la main peut
    changer, `state` donne toujours l'ordre exact.

### Événement

Les clés reprennent celles des fichiers `events/*.json` :

```json
{"id":17,"kind":"PROPAGANDA","faction":"RED","value":3,"base_value":2,"modification":1,"stealth":4}
{"id":52,"kind":"AND","stealth":3,"modification":0,
 "left":{"kind":"PROPAGANDA","faction":"ANY","value":1,"base_value":1,"modification":0,"stealth":1},
 "right":{"kind":"PROPAGANDA","faction":"RED","value":2,"base_value":2,"modification":0,"stealth":2}}
{"id":3,"kind":"ELECTION","stealth":3}
{"id":9,"kind":"MISTAKE","stealth":0,"value":2}
```

- `kind` vaut `PROPAGANDA`, `AND`, `OR`, `ELECTION`, `COLLAPSE`, `EXPLOSION`
  ou `MISTAKE`.
- Propagande : `value` est la valeur actuelle (`base_value` +
  `modification`). Une carte perd sa modification quand elle change de zone.
- ET / OU : `stealth` est la somme des branches ; une modification s'applique
  à la carte entière et à chaque branche.

### Carte grimoire

Une carte grimoire est représentée par son nom, comme dans `decks/*.json` :
`"ETHER"`, `"EUPHORIA"`, etc.

### Contrat

```json
{"name":"RED","factions":["RED"],"rank":1,"points":1}
```

## 3. Questions, réponses et décisions

### Question (moteur → pilote), pour un siège joué à distance

```json
{"type":"ask","id":42,"seat":1,"question":"propaganda_target",
 "context":{"spell":"EUPHORIA","modification":2},
 "options":[{"present":0,"event":{…}},{"present":3,"event":{…}}],
 "state":{ … }}
```

- `id` : entier strictement croissant sur toute la partie, partagé avec les
  décisions des stratégies intégrées (`decided`).
- `seat` : le mage qui doit décider.
- `question` : un des noms du catalogue ci-dessous.
- `context` : ce qui motive la question.
- `options` : liste **non vide** des choix permis.
- `state` : l'état complet de la partie (section 5), toujours présent dans
  une question, même avec `--light`.

### Réponse (pilote → moteur)

```json
{"seat":1,"id":42,"choice":1}
```

`choice` est l'**index de l'option retenue** dans `options`. Ici, c'est la
propagande en position 3 du Présent.

### Décision d'une stratégie intégrée (moteur → pilote)

Quand le moteur joue lui-même un siège (`--seat k=random` ou `smart`), il
n'écrit pas de question : il annonce la décision prise, avec la même forme
et le même compteur d'`id`. Le pilote ne doit pas y répondre.

```json
{"type":"decided","id":57,"seat":2,"question":"reveal_more","context":{"stealth":9,"max_stealth":21},
 "options":["reveal","stop"],"choice":1}
```

Une partie contient donc une suite unique de décisions numérotées, posées
(`ask`) ou annoncées (`decided`) selon qui joue le siège.

### Quand une question est posée

- **Même quand il n'y a qu'une option**, parce que les stratégies tirent au
  hasard même dans ce cas : c'est ce qui garantit qu'une partie jouée à
  distance est identique à la même partie jouée dans le moteur. La couche
  Godot peut répondre seule à ces questions sans déranger le joueur humain.
- Exceptions : `next_shenanigan` n'est posée que s'il y a au moins deux
  cartes manigancées. `propaganda_target` et `opponent_target` ne sont pas
  posées quand il n'y a aucune cible : le sort échoue (`spell_failed`).
  `ether_leak_target` est posée pour chacun des 1 + éther dépensé éthers que
  le sort peut défausser, jusqu'à un `"stop"`, même quand `"stop"` est la
  seule option restante.

### Réponse refusée

Le moteur refuse une réponse dans ces cas :
- ligne qui n'est pas du JSON ;
- `seat`, `id` ou `choice` absent ;
- mauvais `seat` ou mauvais `id` (y compris une réponse envoyée en avance,
  pour une question pas encore posée) ;
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

| `question`          | Quand                                             | `context`                                      | `options`                                                |
|---------------------|---------------------------------------------------|------------------------------------------------|----------------------------------------------------------|
| `reveal_more`       | phase de Révélation, au mage dont c'est le tour   | `{"stealth":s,"max_stealth":m}`                | `["reveal","stop"]`                                      |
| `scheme_card`       | phase de Manigances, main non vide                | —                                              | `[{"hand":i,"card":"EUPHORIA"}, …]`                      |
| `next_shenanigan`   | phase de Sortilèges, plusieurs cartes manigancées | —                                              | `[{"prepared":i,"card":…}, …]`                           |
| `reveal_shenanigan` | phase de Sortilèges, pour chaque carte manigancée | `{"card":…}`                                   | `["reveal","bury"]` (bury : sous le grimoire)            |
| `ether_to_spend`    | un sort révélé                                    | `{"spell":…}`                                  | `[0,1,…,éther disponible]`                               |
| `propaganda_target` | Euphorie / Neurasthénie                           | `{"spell":…,"modification":m}`                 | `[{"present":i,"event":…}, …]`                           |
| `opponent_target`   | Trou de mémoire                                   | `{"spell":"MEMORY_LAPSE"}`                     | `[{"seat":k}, …]`                                        |
| `cosmos_card`       | Renverser l'entropie, une fois par carte reprise  | `{"spell":"ENTROPY_REVERSAL","remaining":r}`   | `[{"cosmos":i,"card":…}, …]`                             |
| `ether_leak_target` | Fuite d'éther, une fois par éther visé            | `{"spell":"ETHER_LEAK","remaining":r}`         | `[{"seat":k,"ether":e}, …, "stop"]`                      |
| `divergence_branch` | résolution d'une propagande OU                    | `{"event":…}`                                  | `["left","right"]`                                       |
| `any_faction`       | résolution d'une propagande ANY                   | `{"value":v}`                                  | `["RED", …]` (factions permises)                         |

`remaining` compte les choix encore possibles, celui-ci compris.

**Secret** : dans le jeu physique, seuls `reveal_shenanigan` et
`scheme_card` sont des choix cachés. Les autres choix se font à la vue de
tous.

## 4. Annonces (moteur → pilote)

Toutes les annonces ont la forme `{"type":…, …, "state":{…}}`. Elles
arrivent **après** le changement qu'elles décrivent : `state` est l'état qui
en résulte. Avec `--light`, les annonces ne portent pas `state` (les
questions le portent toujours) : les messages sont alors bien plus courts.

### Début et fin de partie

| `type`         | Champs                                                                                          | Secret dans le jeu physique                  |
|----------------|-------------------------------------------------------------------------------------------------|----------------------------------------------|
| `game_start`   | `protocol:1`, `seed`, `seats`, `max_stealth`, `hand_size`, `popularity` (`{"RED":0,…}`), `future` (liste complète, dessus d'abord) | `seed` et `future` : de tous            |
| `seat_setup`   | `seat`, `controller` (`{"strategy":"REMOTE"/"RANDOM"/"SMART","greed":g}`), `contract`, `deck` (nom du deck de départ), `hand`, `grimory` (dessus d'abord) | `contract`, `hand` : des autres ; `grimory` : de tous |
| `game_over`    | `finished` (false si `--max-turns` a arrêté la partie), `scores` (par siège), `winner`, `tiebreak` (`none`/`grimory`/`draw`), `contracts` | —            |
| `game_aborted` | `reason`, `seat` (siège fautif, ou `null` si l'entrée standard est fermée). Sans `state`.      | —                                            |
| `error`        | `id` (question concernée), `seat`, `reason`. Sans `state`.                                      | —                                            |

### Tour et phases

| `type`       | Champs                                                              | Secret |
|--------------|---------------------------------------------------------------------|--------|
| `turn_start` | `turn`, `current` (siège), `future_size`                            | —      |
| `phase`      | `name` : `schemes`, `reveal`, `spells`, `resolution` ou `cleanup`   | —      |
| `turn_end`   | `outcome` (`resolved` / `stealth_rollback` / `explosion_rollback`)  | —      |

Un tour annonce `phase` dans cet ordre. Un rollback pendant la Révélation
passe directement de `reveal` à `cleanup` ; un rollback pendant les
Sortilèges (Précipitation) passe de `spells` à `cleanup`.

### Manigances

| `type`         | Champs                          | Secret               |
|----------------|---------------------------------|----------------------|
| `card_schemed` | `seat`, `card` (sortie de la main, posée face cachée) | `card` : des autres |

### Révélation

| `type`                | Champs                                                                                     | Secret |
|-----------------------|--------------------------------------------------------------------------------------------|--------|
| `event_revealed`      | `event`, `present` (position), `stealth` (total), `future_size`                            | —      |
| `collapse`            | `stealth` (porté au seuil : la Révélation s'arrête)                                        | —      |
| `shenanigan_returned` | `seat`, `card` (une carte manigancée revient en main, lors d'un rollback)                  | `card` : des autres |
| `rollback`            | `cause` (`stealth`/`explosion`), `to_oblivion` (l'explosion, ou `null`), `returned` (ids des cartes remises sur le Futur, dans l'ordre de leur révélation : la première est désormais sur le dessus) | —      |

Lors d'un rollback, les `shenanigan_returned` viennent avant `rollback`.

### Sortilèges

| `type`                | Champs                                                         | Secret                      |
|-----------------------|----------------------------------------------------------------|-----------------------------|
| `shenanigan_buried`   | `seat`, `card` (placée sous le grimoire), `grimory_size`       | `card` : des autres         |
| `shenanigan_revealed` | `seat`, `card` (elle se résout ensuite)                        | —                           |
| `ether_stored`        | `seat`, `ether` (total dans le repaire)                        | —                           |
| `ether_spent`         | `seat`, `amount` (vers le Cosmos), `ether` (restant)           | —                           |
| `spell_failed`        | `seat`, `card` (aucune cible : pas d'éther dépensé)            | —                           |
| `spell_discarded`     | `seat`, `card` (le sort part dans le Cosmos), `cosmos_size`    | —                           |
| `choice_cancelled`    | `immune` (siège), `by` (siège), `charges_left`                 | —                           |
| `propaganda_modified` | `present`, `modification`, `event` (après modification)        | —                           |
| `memory_lapse`        | `from`, `to` (la carte passe dans la main de `to`)             | —                           |
| `cosmos_taken`        | `seat`, `card` (du Cosmos vers la main)                        | —                           |
| `ether_leaked`        | `seat` (qui perd l'éther, vers le Cosmos), `by`, `ether` (restant) | —                       |
| `immunity_gained`     | `seat`, `gained`, `charges` (total)                            | —                           |
| `future_read`         | `seat`, `events` (dessus du Futur d'abord)                     | `events` : des autres       |
| `event_skipped`       | `event` (du dessus du Futur vers le Passé), `future_size`      | —                           |
| `forced_reveal`       | `count` (Précipitation : jusqu'à `count` `event_revealed` suivent) | —                       |

Un sort révélé produit, dans l'ordre : `shenanigan_revealed`, les décisions
et effets du sort, `ether_spent` s'il a agi avec de l'éther (ou
`spell_failed`), puis `spell_discarded`. Exceptions : un éther produit
`ether_stored` ; un Trou de mémoire qui atteint sa cible produit
`memory_lapse` (la carte change de main au lieu d'aller au Cosmos).

### Résolution

| `type`           | Champs                                                                                     | Secret |
|------------------|--------------------------------------------------------------------------------------------|--------|
| `election`       | `points` (gagnés par siège), `scores` (totaux), `popularity`                               | —      |
| `event_resolved` | `event` (avec ses modifications ; il est maintenant dans le Passé, sans elles), `applied` (`[{"faction":"RED","value":2}, …]` après choix OU / ANY), `popularity` | —      |

Une élection produit `election`, puis son `event_resolved`.

### Fin de tour (`cleanup`)

| `type`             | Champs                                              | Secret             |
|--------------------|-----------------------------------------------------|--------------------|
| `cards_drawn`      | `seat`, `cards` (ajoutées à la fin de la main), `grimory_size` | `cards` : des autres |
| `immunity_expired` | `seat` (ses charges d'immunité sont perdues)        | —                  |

## 5. L'état de la partie (`state`)

```json
{
  "seed": 1234, "turn": 7, "current": 0, "phase": "reveal",
  "max_stealth": 21, "stealth": 9,
  "popularity": {"RED": 4, "YELLOW": 2, "BLUE": 5},
  "present":  [ <événement>, … ],
  "future":   [ <événement>, … ],
  "past":     [ <événement>, … ],
  "oblivion": [ <événement>, … ],
  "cosmos":   ["EUPHORIA", "ETHER", …],
  "contracts_left": 3,
  "seats": [
    {"seat": 0, "controller": {"strategy": "REMOTE", "greed": 0}, "deck": "implemented",
     "contract": <contrat>, "score": 2,
     "hand": ["ETHER", …], "hand_limit": 4, "prepared": ["EUPHORIA"],
     "ether": 1, "immunity": 0, "grimory": ["ETHER", …], "known_future": [17, 52]},
    …
  ]
}
```

- `future` est dans l'ordre réel de la pile, dessus d'abord.
- `grimory` est dans l'ordre de pioche, dessus d'abord.
- `prepared` : les cartes manigancées pas encore traitées ce tour.
- `known_future` liste les `id` des cartes du Futur que ce mage a vues avec
  Lecture, dessus d'abord. Ce sont les seules cartes du Futur qu'un bot
  honnête peut utiliser.
- `contracts_left` : contrats non distribués (face cachée).

## 6. Exemple de début de partie (3 mages, siège 2 joué par le moteur)

Les `state` sont omis.

```
→ {"type":"game_start","protocol":1,"seed":1234,"seats":3,"max_stealth":21,"hand_size":4,"popularity":{"RED":0,"YELLOW":0,"BLUE":0},"future":[…]}
→ {"type":"seat_setup","seat":0,"controller":{"strategy":"REMOTE","greed":0},"contract":{…},"deck":"implemented","hand":["ETHER","EUPHORIA","ETHER","MEMORY_LAPSE"],"grimory":[…]}
→ {"type":"seat_setup","seat":1, …}
→ {"type":"seat_setup","seat":2,"controller":{"strategy":"SMART","greed":10}, …}
→ {"type":"turn_start","turn":0,"current":0,"future_size":128}
→ {"type":"phase","name":"schemes"}
→ {"type":"ask","id":0,"seat":0,"question":"scheme_card","context":{},"options":[{"hand":0,"card":"ETHER"}, …],"state":{…}}
← {"seat":0,"id":0,"choice":1}
→ {"type":"card_schemed","seat":0,"card":"EUPHORIA"}
→ {"type":"ask","id":1,"seat":1,"question":"scheme_card", …}
← {"seat":1,"id":1,"choice":0}
→ {"type":"card_schemed","seat":1,"card":"ETHER"}
→ {"type":"decided","id":2,"seat":2,"question":"scheme_card","context":{},"options":[…],"choice":3}
→ {"type":"card_schemed","seat":2,"card":"READING"}
→ {"type":"phase","name":"reveal"}
→ {"type":"ask","id":3,"seat":0,"question":"reveal_more","context":{"stealth":0,"max_stealth":21},"options":["reveal","stop"],"state":{…}}
← {"seat":0,"id":3,"choice":0}
→ {"type":"event_revealed","event":{"id":88,"kind":"PROPAGANDA","faction":"BLUE","value":2,"base_value":2,"modification":0,"stealth":4},"present":0,"stealth":4,"future_size":127}
```

## 7. Lancement

```
./server [--players <n>] [--seat <k>=remote|random[:greed]|smart[:greed]]...
         [--deck <file.json>]... [--contracts <file.json>] [--events <file.json>]
         [--popularity <n>] [--max-stealth <n>] [--max-turns <n>] [--seed <n>]
         [--light] [--quiet]
```

- `--players` : nombre de mages (3 par défaut).
- `--seat` : qui joue un siège. `remote` (par défaut) : questions sur la
  sortie standard. `random` ou `smart` : la stratégie de la simulation,
  jouée par le moteur, dont les décisions sont annoncées (`decided`) ;
  avidité 10 par défaut.
- `--deck`, `--contracts`, `--events`, `--popularity`, `--seed` : comme
  `./simulation`. Les contrats sont tirés au hasard à partir de la graine.
- `--max-stealth` : seuil de furtivité (21 par défaut).
- `--max-turns` : arrête la partie après ce nombre de tours (0 par défaut :
  pas de limite) ; `game_over` porte alors `finished:false`.
- `--light` : les annonces ne portent pas `state`.
- `--quiet` : rien sur la sortie d'erreur.

La partie commence dès le lancement. Elle se termine quand le Futur est
vide : le moteur envoie alors `game_over` et s'arrête avec le code de
sortie 0. Une option invalide donne le code 1, sans rien écrire sur la
sortie standard.

À graine égale, avec les mêmes fichiers, `./server` joue la même partie que
`./simulation` quand ses sièges sont `random:10`.

## 8. Bots de test

Le programme `./bot --seat k [--strategy random|smart] [--greed g]` lit les
messages du moteur et répond aux questions de son siège, comme la même
stratégie le ferait dans le moteur. Il reconstruit la partie à partir du
`state` de chaque question, mais ne lit que ce que son mage peut savoir :
- les zones face visible ;
- sa propre main, ses cartes manigancées et son contrat ;
- les tailles des mains et des grimoires adverses ;
- les cartes du Futur listées dans son `known_future`.

Le script `scripts/play_with_bots.py` lance le moteur et un bot par siège
demandé, puis transmet à chaque bot les messages du moteur et au moteur les
réponses des bots :

```
scripts/play_with_bots.py --bot 0=smart --bot 1=random --bot 2=random:14 \
    --transcript partie.jsonl -- --seed 7 --deck decks/all_spells.json
```

À graine égale, une partie jouée par des bots est identique à la même partie
jouée par les mêmes stratégies dans le moteur (`--seat k=…`) : mêmes
annonces, et une question `ask` là où le moteur annonce un `decided`, avec
le même `id` et le même choix (voir `tests/integration/test_server.py`).
