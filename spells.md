# Manigances

Lors de chaque tour (y compris ceux de vos adversaires), vous devez, si vous le pouvez, placer une carte manigance de votre main devant vous face cachée. Lors de la phase Sortilège, dans l'ordre du tour, chaque mage choisit de placer sa carte manigance sous son grimoire ou de la révéler, auquel cas iel en applique les effets.

## Règles communes

- La notation « N [+ éther] » équivaut à « N plus le nombre d'éthers révélés que vous défaussez dans le **Cosmos** ». Lae mage choisit combien d'éthers iel dépense au moment de révéler son sort.
- Un **Sort** révélé applique son effet si possible, puis il est défaussé dans le **Cosmos**.
- Si l'effet ne peut pas s'appliquer (rien à cibler, pile vide…), le sort est quand même défaussé dans le **Cosmos**, mais l'éther n'est pas dépensé.
- Si une pile contient moins de cartes que l'effet n'en demande (**Cosmos**, **Futur**, éthers adverses…), le sort agit sur toutes les cartes disponibles. Il n'échoue que s'il n'y en a aucune.
- Les modifications de popularité (Euphorie et Neurasthénie Collective) s'additionnent. Une carte perd toutes ses modifications dès qu'elle change de zone : quand elle est résolue et passe dans le **Passé**, ou quand un **Rollback** la remet dans le **Futur**.
- Un sort annulé par une **Immunité parlementaire cosmique** compte comme lancé : il est défaussé dans le **Cosmos** avec l'éther dépensé.

## Cartes manigance implantées

Entre parenthèses : le nom de la carte dans les fichiers de deck (`decks/*.json`).

- Éther (`ETHER`) : *Éther* à placer face visible dans sa réserve personnelle d'éther, pour le défausser lors de l'exécution d'un sort afin de le rendre plus puissant. Chaque éther vaut 1.

- Euphorie Collective (`EUPHORIA`) : *Sort* qui augmente de 1 [+ éther] les valeurs de popularité de la carte propagande du **Présent** de votre choix. Sur une propagande multiple (ET / OU), toutes les valeurs de la carte sont modifiées.

- Neurasthénie Collective (`NEURASTHENIA`) : *Sort* qui diminue de 1 [+ éther] les valeurs de popularité de la carte propagande du **Présent** de votre choix. Sur une propagande multiple (ET / OU), toutes les valeurs de la carte sont modifiées.

- Trou de mémoire contagieux (`MEMORY_LAPSE`) : *Sort* qui ne coûte aucun éther et qui a pour effet de se renvoyer dans la main de l'adversaire de votre choix. Il n'est pas défaussé dans le **Cosmos**, sauf s'il ne peut viser personne ou si sa cible est immunisée.

- Précipitation des événements (`PRECIPITATION`) : *Sort* qui force la personne dont c'est le tour à révéler 1 [+ éther] événements supplémentaires. Les règles de la phase de Révélation s'appliquent à ces événements (Rollback si la furtivité dépasse X, Explosion, Effondrement).

- Lecture (`READING`) : *Sort* qui permet de regarder secrètement les 1 [+ éther] événements du dessus du **Futur** puis de les y replacer dans le même ordre. Lae mage garde en mémoire les événements vus.

- Renverser l'entropie (`ENTROPY_REVERSAL`) : *Sort* qui permet d'ajouter à sa main 1 [+ éther] cartes de votre choix depuis le **Cosmos**. Le sort lui-même et l'éther dépensé pour le lancer n'arrivent dans le **Cosmos** qu'après l'effet : ils ne peuvent pas être repris par ce sort.

- Fuite d'éther (`ETHER_LEAK`) : *Sort* qui permet de défausser dans le **Cosmos** jusqu'à 1 [+ éther] cartes d'éther révélées par vos adversaires, choisies une par une. Lae mage peut s'arrêter avant la limite. Le sort échoue si aucun adversaire n'a d'éther révélé.

- Immunité parlementaire cosmique (`IMMUNITY`) : *Sort* qui annule les 1 [+ éther] prochaines fois où vous, un sort, un éther, un modificateur ou un événement que vous contrôlez seriez choisi.e ce tour-ci. Seuls les choix de vos adversaires sont annulés, chacun consommant une utilisation. Les utilisations restantes sont perdues à la fin du tour. Est concerné.e :
	- vous-même, quand Trou de mémoire contagieux vous désigne ;
	- vos éthers révélés, quand Fuite d'éther les vise ;
	- les événements du **Présent** pendant votre tour (c'est vous qui les contrôlez), quand Euphorie ou Neurasthénie Collective les vise.

- Sauter un chapitre (`CHAPTER_SKIP`) : *Sort* qui permet de placer les 1 [+ éther] prochaines cartes du **Futur** dans le **Passé**, sans les résoudre. Une Explosion placée ainsi va dans le **Passé** et non dans l'**Oubli**.


## Cartes manigance à implanter

- Aucune pour le moment.
