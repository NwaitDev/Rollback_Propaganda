# Rollback propaganda

Vous êtes un.e mage de l'ombre manipulant les événements à sa guise pour tirer les ficelles du gouvernement selon ses intérêts personnels. Cependant, si vous manquez de discretion pendant que vous mettez à exécution vos dessins, vous serez obligé.e de remonter le temps pour annuler vos manigances et rester patte blanche auprès du conseil cosmique.

## Mise en place

Distribuez au hasard une carte **Contrat** à chaque mage qu'iel consulte secrètement puis dispose face cachée près d'ellui. Les cartes **Contrat** restantes sont mélangées et placées face cachée au centre de la table.
Mélanger les cartes événements en une pile face cachée au centre de la table : cette pile constitue le **Futur** (cartes du futur à définir en fonction du nombre de mages). Réserver un espace au centre de la table pour disposer une ligne d'environ 5 ou 6 cartes. Il s'agit du **Présent**. Réserver un espace au centre de la table pour former la défausse des événements, le **Passé**. Enfin, deux derniers espaces doivent être réservés pour former deux défausses, le **Cosmos** (pour les cartes grimoires utilisées) et l'**Oubli** (usuellement pour les cartes explosions révélées).

Placer le score de popularité de chaque faction à X (valeur de départ à définir).
Placer le score de chaque mage à 0.

Chaque mage reçoit un ensemble prédéfini de cartes grimoire qu'iel mélange et dispose face cachée en une pile près d'ellui, le **Grimoire Personnel** (cartes grimoires de départ à définir).

Chaque mage pioche 4 cartes de son **Grimoire Personnel**.

La personne la plus manipulatrice commence, sinon c'est la personne qui connaît le mieux les règles (ce qui revient au même).

## Déroulement d'un tour

Un tour se déroule en 2 ou 4 phases, dans l'ordre suivant :

- Phase de Manigances,
- Phase de Révélation,
- Phase de Sortilèges,
- Phase de Résolution.

### Phase de Manigances

Chaque mage choisit une carte de sa main qu'iel **Manigance** (iel dispose la carte face cachée devant ellui).

### Phase de Révélation

Lae mage dont c'est le tour révèle la carte du dessus du **Futur** et l'ajoute à la suite du **Présent**. 
Iel peut répéter cette action autant de fois qu'iel le désire, cependant :

- Si la valeur totale de furtivité dépasse strictement X, **Rollback** (Remettez les cartes du **Présent** sur le **Futur** dans l'ordre inverse où elles ont été révélées (de sorte qu'elles soient dépilées dans le même ordre au prochain tour). Les mages autour de la table reprennent en main leur carte manigançée ce tour-ci. Le tour se termine sans effectuer les phases suivante.)

- Si une carte explosion est révélée, placez-la dans l'**Oubli** puis **Rollback**.

Si lae mage dont c'est le tour ne souhaite plus révéler de carte, iel annonce "Sortilèges".

### Phase de Sortilèges

Dans le sens des aiguilles d'une montre et en commençant par lae mage dont c'est le tour, pour chaque carte manigançée par ellui, dans l'ordre de son choix, chaque mage décide de :

- placer sa carte manigançée en dessous de son **Grimoire Personnel**,
- retourner sa carte manigancée,
	- Si c'est un **Sort**, lae mage en applique l'effet si possible, puis le défausse dans le cosmos,
	- Si c'est un **Éther** ou un **Modificateur** (noms à discuter), iel la garde face visible près d'ellui.

Quand chaque mage a effectué l'une de ces deux actions pour chacune de ses cartes manigancées, on passe à la phase résolution.

### Phase de Résolution

En suivant l'ordre de révélation, lae mage dont c'est le tour résout les effets des événements du **Présent** puis les défausse dans le **Passé**.

Quand tous les événements sont résolus, chaque mage qui possède moins de 4 cartes en main pioche depuis son **Grimoire Personnel** jusqu'à avoir 4 cartes en main.

## Effets des cartes événements

- Propagande : Augmente ou diminue la popularité d'une ou plusieurs factions.
- Élection : Chaque mage révèle son contrat. Les mages possédant un contrat avec la faction la plus populaire marquent un point. Les contrats sont ensuite à nouveau mis face cachée.
- Explosion : *Doit être résolu dès qu'il est révélé puis oublié.* Provoque un **Rolback**.
- Éfondrement : *Doit être résolu dès qu'il est révélé.* Force à passer en Phase de Sortilèges.
- Inévitable erreur: n'a aucun effet.

## Fin de la partie

La partie s'arrête dans le cas où un tour se termine et qu'il n'y a plus de carte dans le **futur**.

Lae mage avec le score le plus élevé remporte la partie. En cas d'égalité, c'est lae mage avec le plus de cartes restantes dans son grimoire qui l'emporte. Si l'égalité persiste, lae gagnant.e est tiré.e au sort parmi ces mages.

## Idées Pêle-mêles

- flèches sur les cartes événements pour retirer l'ambiguïté sur l'ordre de résolution des événements.

- Utiliser très précisément le terme "choix" sur toutes les cartes qui implique un ciblage de la part d'un mage.

- La notation "N [+ éther]" équivaut à "N plus le nombre d'éther révélés que vous défaussez dans le **Cosmos**".

- Crise de la confiance : *Événement* qui permet d'échanger secrètement les contrats de deux mages de votre choix, ou d'un mage de votre choix avec une carte contrat non-attribuées.

- Espionnage : *Événement* qui permet de regarder secrètement le contrat du mage de votre choix.

- Retournement de situation : *Événement qui doit être résolu dès qu'il est révélé.* inverse l'ordre de résolution des événements à la phase de Résolution.

-------------------
Choses à implanter:

- finir les effets des cartes manigances pour le random player
- les effets des cartes manigances pour le smart player
- pipeline de simulation et acquisition de données

-------------------
Optimisations à effectuer sur le modèle : 

- mettre des smart pointers partout où ce serait utile