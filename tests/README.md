# Tests

Deux familles de tests, lancées ensemble par `make test` depuis la racine du projet :

| Dossier | Langage | Ce qui est testé |
|---|---|---|
| `tests/*.cpp` | C++ (doctest) | les règles, carte par carte, sur des parties construites à la main ; le joueur intelligent ; les métriques ; les fichiers de configuration ; des propriétés vérifiées sur des centaines de parties complètes |
| `tests/integration/` | Python (pytest) | les programmes `./simulation` et `./campaign` : options et messages d'erreur, reproductibilité, fichiers CSV d'une petite campagne, sorties de référence de 24 parties complètes |

Les tests du tableau de bord sont à part, dans `dashboard/tests/` (`make dashboard-test`).
`make test-all` lance tout : `make test`, `make check` et `make dashboard-test`.

Prérequis : g++ (C++17), make, Python 3 avec pytest (`pip install pytest`).
doctest (version 2.4.11, licence MIT) est fourni dans `third_party/doctest/doctest.h`, rien à installer.

## Lancer les tests

```sh
make test                                   # tout (une dizaine de secondes)
make test TEST_ARGS='-tce="*[slow]*"'       # sans les tests lents sur des parties complètes
./build/check/unit_tests -tc="*contract*"   # seulement les tests C++ dont le nom contient "contract"
./build/check/unit_tests -sf="*test_rules*" # seulement ceux d'un fichier
./build/check/unit_tests --success          # affiche aussi les vérifications réussies
python3 -m pytest -q tests/integration -k replay   # seulement les tests Python contenant "replay"
```

Les tests C++ sont compilés avec AddressSanitizer et UBSan (comme `make check`) :
une fuite mémoire ou un comportement indéfini fait échouer le test.
Ils lisent les fichiers du projet (`decks/`, `events/`, `contracts/`) : lancez-les depuis la racine.

Quand une vérification échoue, doctest affiche le fichier, la ligne, l'expression et
les valeurs comparées, ainsi que les valeurs `CAPTURE(...)` en cours (le deck, la carte...).

## Ajouter un test C++

Ajoutez un `TEST_CASE` dans le fichier qui correspond au sujet, ou créez un nouveau
fichier `tests/test_<sujet>.cpp` : le makefile compile tous les `tests/*.cpp`.

```cpp
#include "test_support.hpp"

TEST_CASE("une propagande révélée ajoute sa valeur à sa faction") {
	test::quiet quiet;                                 // coupe l'affichage de la partie
	game g = test::make_game<random_player>(1);       // 3 joueurs, contrats RED / YELLOW / BLUE
	// variation des popularités quand le joueur 0 résout l'événement
	std::vector<int> d = test::delta(g, event(RED, 2, 1), 0, {0, 0, 0});   // RED, valeur 2, discrétion 1
	CHECK(d == std::vector<int>{2, 0, 0});
	REQUIRE(g.players.size() == 3);                   // REQUIRE arrête le test s'il échoue
}
```

Ce qui est disponible (voir `test_support.hpp`) :

- `test_support.hpp` rend publics les membres privés des classes du jeu : un test peut
  placer directement des cartes dans une main, une pile ou le futur ;
- `test::make_game<Joueur>(graine, popularités, avidité)`, `test::make_contract(...)`,
  `test::delta`, `test::hand_of`, `test::give_ether`, `test::reveal`, `test::grimory_cards` ;
- `test::scripted` : un joueur dont on fixe les décisions (révéler ou non, adversaire visé...) ;
- `test::temporary_file(contenu, nom)` : écrit un fichier JSON temporaire (deck, expérience...) ;
- `test::error_of([&] { ... })` : le message de l'exception levée, vide s'il n'y en a pas.

Mettez `[slow]` au début du nom d'un test qui joue beaucoup de parties, pour qu'on puisse l'écarter.

## Ajouter un test Python

Ajoutez une fonction `test_...` dans `tests/integration/test_programs.py` (ou un nouveau
fichier `tests/integration/test_<sujet>.py`). La fonction `run(SIMULATION, ...)` lance
un programme depuis la racine et renvoie sa sortie (`.stdout`, `.stderr`, `.returncode`) ;
la fixture `campaign` fournit une petite campagne déjà jouée dans un dossier temporaire
(jamais dans `results/`).

```python
def test_une_graine_donne_toujours_le_meme_gagnant():
    a = run(SIMULATION, "--play", "--seed", 42)
    b = run(SIMULATION, "--play", "--seed", 42)
    assert a.stdout == b.stdout
```

## Sorties de référence

`tests/integration/golden/` contient la sortie complète de 24 parties
(4 decks × 6 graines, contrats `contracts/mixed.json`). Le test vérifie qu'elles sont
reproduites à l'octet près : il détecte tout changement involontaire du déroulement des parties.

Après un changement **voulu** des règles ou des décisions des joueurs, ces tests échouent
normalement. Régénérez alors les références et relisez les différences :

```sh
make golden
diff -r ancien_dossier/tests/integration/golden tests/integration/golden   # ou git diff
```
