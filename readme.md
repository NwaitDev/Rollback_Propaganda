# Rollback propaganda

Vous êtes un.e mage de l'ombre manipulant les événements à sa guise pour tirer les ficelles du gouvernement selon ses intérêts personnels. Cependant, si vous manquez de discretion pendant que vous mettez à exécution vos dessins, vous serez obligé.e de remonter le temps pour annuler vos manigances et rester patte blanche auprès du conseil cosmique.

## Mise en place

Mélangez les cartes **Contrat** et distribuez-en une au hasard à chaque mage, qui la consulte secrètement puis la dispose face cachée près d'ellui. Les cartes **Contrat** restantes sont placées face cachée au centre de la table.
Mélanger les cartes événements en une pile face cachée au centre de la table : cette pile constitue le **Futur** (cartes du futur à définir en fonction du nombre de mages). Réserver un espace au centre de la table pour disposer une ligne d'environ 5 ou 6 cartes. Il s'agit du **Présent**. Réserver un espace au centre de la table pour former la défausse des événements, le **Passé**. Enfin, deux derniers espaces doivent être réservés pour former deux défausses, le **Cosmos** (pour les cartes grimoires utilisées) et l'**Oubli** (pour les cartes Explosion révélées).

Placer le score de popularité de chaque faction à X (valeur de départ à définir).
Placer le score de chaque mage à 0.

Chaque mage reçoit un ensemble prédéfini de cartes grimoire qu'iel mélange et dispose face cachée en une pile près d'ellui, le **Grimoire Personnel** (cartes grimoires de départ à définir).

Chaque mage pioche 4 cartes de son **Grimoire Personnel**.

La personne la plus manipulatrice commence, sinon c'est la personne qui connaît le mieux les règles (ce qui revient au même).

## Cartes Contrat

Chaque contrat désigne une ou plusieurs factions, un rang de popularité et un nombre de points. À chaque **Élection**, il rapporte ses points si l'une de ses factions occupe ce rang.

Une faction n'occupe un rang que si elle y est seule : deux factions à égalité n'occupent aucun rang. Par exemple :

- popularités 5, 5 et 2 : aucune faction n'est en tête, et la troisième reste troisième (elle n'est pas deuxième) ;
- popularités 5, 3 et 3 : la première est en tête, et aucune faction n'est deuxième.

Types de contrats :

| Contrat | Condition | Points |
|---|---|---|
| Partisan | la faction désignée est en tête | 1 |
| Favori | la faction désignée est en tête | 2 |
| Outsider | la faction désignée est deuxième | 3 |
| Duo | l'une des deux factions désignées est en tête | 1 |

## Déroulement d'un tour

Un tour se déroule en 2 ou 4 phases, dans l'ordre suivant :

- Phase de Manigances,
- Phase de Révélation,
- Phase de Sortilèges,
- Phase de Résolution.

### Phase de Manigances

Chaque mage choisit une carte de sa main qu'iel **Manigance** (iel dispose la carte face cachée devant ellui). Un.e mage qui n'a plus de carte en main ne manigance rien.

### Phase de Révélation

Lae mage dont c'est le tour révèle la carte du dessus du **Futur** et l'ajoute à la suite du **Présent**.
Iel peut répéter cette action autant de fois qu'iel le désire, cependant :

- Si la valeur totale de furtivité dépasse strictement X, **Rollback**.

- Si une carte Explosion est révélée, placez-la dans l'**Oubli** puis **Rollback**.

- Si une carte Effondrement est révélée, la phase de Révélation s'arrête et on passe en phase de Sortilèges.

Si lae mage dont c'est le tour ne souhaite plus révéler de carte, iel annonce "Sortilèges".

### Rollback

Remettez les cartes du **Présent** sur le **Futur** dans l'ordre inverse où elles ont été révélées (de sorte qu'elles soient dépilées dans le même ordre au prochain tour). Les cartes perdent alors leurs modifications de popularité. Les mages reprennent en main les cartes qu'iels ont manigancées ce tour-ci et qu'iels n'ont pas encore révélées.

Le tour se termine sans effectuer les phases suivantes : chaque mage qui possède moins de 4 cartes en main pioche depuis son **Grimoire Personnel** jusqu'à avoir 4 cartes, puis c'est au tour de lae mage suivant.e.

Un **Rollback** peut aussi survenir pendant la phase de Sortilèges, quand un sort fait révéler des événements (Précipitation des événements). Les sorts déjà résolus restent résolus.

### Phase de Sortilèges

Dans le sens des aiguilles d'une montre et en commençant par lae mage dont c'est le tour, pour chaque carte manigancée par ellui, dans l'ordre de son choix, chaque mage décide de :

- placer sa carte manigancée en dessous de son **Grimoire Personnel**,
- retourner sa carte manigancée,
	- Si c'est un **Sort**, lae mage en applique l'effet si possible, puis le défausse dans le **Cosmos**,
	- Si c'est un **Éther**, iel la garde face visible près d'ellui.

Les effets des cartes manigances et les règles communes aux sorts sont décrits dans `spells.md`.

Quand chaque mage a effectué l'une de ces deux actions pour chacune de ses cartes manigancées, on passe à la phase de Résolution.

### Phase de Résolution

En suivant l'ordre de révélation, lae mage dont c'est le tour résout les effets des événements du **Présent** puis les défausse dans le **Passé**, où ils perdent leurs modifications de popularité.

Quand tous les événements sont résolus, chaque mage qui possède moins de 4 cartes en main pioche depuis son **Grimoire Personnel** jusqu'à avoir 4 cartes en main.

## Effets des cartes événements

- Propagande : Augmente ou diminue la popularité d'une ou plusieurs factions.
	- Une propagande « au choix » (ANY) est attribuée par lae mage dont c'est le tour à la faction de son choix. Sur une même carte, elle ne peut pas viser une faction déjà visée par la carte, sauf si toutes les factions le sont déjà.
	- Une propagande multiple « ET » applique ses deux propagandes.
	- Une propagande multiple « OU » n'en applique qu'une, choisie par lae mage dont c'est le tour.
- Élection : Chaque mage révèle son contrat et marque les points qu'il rapporte (voir **Cartes Contrat**). Les contrats sont ensuite à nouveau mis face cachée.
- Explosion : *Doit être résolue dès qu'elle est révélée puis oubliée.* Provoque un **Rollback**.
- Effondrement : *Doit être résolu dès qu'il est révélé.* Force à passer en Phase de Sortilèges.
- Inévitable erreur : n'a aucun effet.

## Fin de la partie

La partie s'arrête dans le cas où un tour se termine et qu'il n'y a plus de carte dans le **Futur**.

Lae mage avec le score le plus élevé remporte la partie. En cas d'égalité, c'est lae mage avec le plus de cartes restantes dans son grimoire qui l'emporte. Si l'égalité persiste, lae gagnant.e est tiré.e au sort parmi ces mages.

## Idées Pêle-mêles

- flèches sur les cartes événements pour retirer l'ambiguïté sur l'ordre de résolution des événements.

- Utiliser très précisément le terme "choix" sur toutes les cartes qui implique un ciblage de la part d'un mage.

- Crise de la confiance : *Événement* qui permet d'échanger secrètement les contrats de deux mages de votre choix, ou d'un mage de votre choix avec une carte contrat non-attribuées.

- Espionnage : *Événement* qui permet de regarder secrètement le contrat du mage de votre choix.

- Retournement de situation : *Événement qui doit être résolu dès qu'il est révélé.* inverse l'ordre de résolution des événements à la phase de Résolution.

## Questions ouvertes

- Partie bloquée : si les mages révèlent toujours au-delà du seuil de furtivité, chaque tour finit en **Rollback** sur les mêmes cartes dans le même ordre, et la partie ne se termine jamais. Il faudra une règle qui fait évoluer la situation après un Rollback forcé (par exemple, la carte fautive passe sous le **Futur**).

- Combien de cartes un.e mage peut-iel manigancer par tour ? La phase de Manigances en fait poser une, mais la phase de Sortilèges parle de « chaque carte manigancée ». La simulation accepte plusieurs manigances par mage.

-------------------

# Simulation

Le code C++ simule des parties entre mages contrôlés par des stratégies :

- **random player** : fait tous ses choix au hasard et révèle des événements tant que la furtivité reste sous son avidité ;
- **smart player** : choisit selon des heuristiques et projette, pour chaque choix qui touche aux popularités, la note qu'en donnerait son contrat ; il exploite aussi ce qu'il a vu avec Lecture pour s'arrêter avant une Explosion ou un dépassement du seuil.

## Compiler et lancer

- `make` compile les quatre programmes : `./simulation` (une partie), `./campaign` (une campagne de parties), `./server` (le moteur de jeu piloté par le protocole JSON, voir ci-dessous) et `./bot` (une stratégie de la simulation qui joue un siège du serveur).
- `make play` joue une partie complète et affiche le gagnant. Options : `--deck`, `--contracts`, `--events`, `--popularity`, `--seed` (une même graine rejoue la même partie).
- `make experiment EXP=experiments/base.json` joue une campagne et écrit ses résultats dans `results/<nom>/`.
- `make check` compile avec les sanitizers et `-Werror`, puis joue des parties de test avec chaque deck, une petite campagne (`experiments/smoke.json`) et des parties du serveur.
- `make valgrind` joue une partie sous valgrind.
- `make test` lance les tests (règles, joueur intelligent, métriques, fichiers, protocole, programmes) ; `make test-all` y ajoute `make check` et les tests du tableau de bord. Les tests Python utilisent `./bin/python3` (l'environnement virtuel) ; ailleurs : `make test PYTHON=python3`. Pour lancer une partie des tests ou en écrire de nouveaux, voir `tests/README.md`.

## Moteur de jeu (serveur)

`./server` joue une partie et dialogue sur ses entrée et sortie standard en JSON (une ligne par message) : il annonce tout ce qui se passe, avec l'état complet de la partie, pose les questions des sièges joués à distance et annonce les décisions des sièges qu'il joue lui-même (`--seat 1=smart`). Le protocole est décrit dans `docs/protocol.md`.

- `./server --seat 0=remote --seat 1=smart --seat 2=random --seed 7` : une partie où le siège 0 répond sur l'entrée standard.
- `scripts/play_with_bots.py --bot 0=smart --bot 1=random --bot 2=random -- --seed 7` : une partie où chaque siège est joué par un processus `./bot`.

À graine égale, le serveur joue la même partie que `./simulation`, et un `./bot` décide comme la même stratégie jouée dans le moteur.

## Fichiers de configuration

- `decks/` : grimoires de départ, avec le nombre d'exemplaires de chaque carte manigance.
- `contracts/` : piles de cartes Contrat (factions, rang, points, nombre d'exemplaires).
- `events/` : cartes événements du **Futur**.
- `experiments/` : campagnes. Une campagne fixe le nombre de parties entre random players et avec un smart player, le nombre de joueurs, l'avidité des random players (un niveau ou une liste, par exemple `[8, 13, 17]` : les parties sont jouées pour chaque niveau, avec les mêmes graines) et celle du smart player, la taille de la main, le deck, les événements, les contrats et la graine. La clé facultative `extra_card` ajoute une carte au grimoire d'un seul joueur (le smart player dans les parties qui en ont un).

Une campagne écrit dans `results/<nom>/` :

- `games.csv` : une ligne par partie (niveau d'avidité, graine, gagnant, contrats, points, popularités finales) ;
- `campaigns.csv` et `details.csv` : les métriques agrégées par niveau d'avidité et type de partie, décrites dans `metrics/columns.csv` ;
- une copie de son fichier d'expérience.

Une partie précise se rejoue avec `./campaign <experiment.json> --replay <random_only|with_smart> <index> [<avidité>]`.

## Tableau de bord

Le tableau de bord (`dashboard/`) explore les métriques de toutes les campagnes de `results/` et lance de nouvelles campagnes.

- Installation, une fois : `python3 -m venv .venv && .venv/bin/pip install -r dashboard/requirements.txt`
- Lancement : `make dashboard PYTHON=.venv/bin/python` (ou `make dashboard` si les paquets sont installés pour `python3`), puis ouvrir l'adresse affichée.
- Tests : `make dashboard-test` joue de petites campagnes de test dans `build/dashboard_test/` (jamais dans `results/`) et vérifie le tableau de bord dessus.

Page « Explorer les résultats » :

- on choisit une métrique, puis une ou plusieurs valeurs pour chaque paramètre ;
- tous les paramètres fixés : une valeur ; un paramètre libre : un graphique 2D ; deux paramètres libres : un graphique 3D (et sa vue de dessus) ; au-delà, rien n'est représenté ;
- chaque valeur a son intervalle de confiance à 95 % ; un cas jamais étudié est signalé ;
- les campagnes qui ont exactement les mêmes paramètres sont fusionnées ;
- avec un témoin choisi dans le menu, chaque point est comparé à la ligne du témoin de même avidité et de même type de partie, et l'écart est dit significatif ou non (p < 0,05) ;
- le tableau des valeurs de chaque graphique se télécharge en CSV.

Page « Lancer une campagne » : un formulaire reprend tous les paramètres d'une expérience, écrit `experiments/<nom>.json` et lance `./campaign`.

-------------------
Choses à implanter :

- une règle contre les parties bloquées (voir **Questions ouvertes**)
