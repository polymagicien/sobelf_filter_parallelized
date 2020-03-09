# Rapport 

## MPI
- **Division du travail par image** (une image par process)
  - Alloc et copie du header et des pixels dans un nouvel espace mémoire avant envoi (But : limiter les communications mais malloc gourmand)
  - Envoi préalable d'un header puis envoie des pixels (technique adoptée)
- On a fait quelques tests
  - Temps d'envoi de grandes images entre deux process au sein d'un même noeud et inter noeud (facteur de 100 environ)

Maintenant, on veut **diviser le travail au sein d'une image**
- Problématique : pixels en lignes. Comment partager le travail équitablement (colonnes) si la mémoire n'est pas contigue?
  - Hypothèse 1 : Trouver le rapport entre le temps passer sur les pixels à flouter, et le temps passer sur les pixels "centraux"
    - Test : non concluant, puisqu'on ne sait pas combien d'itérations de blur sont nécéssaires. Dans le jeu de donnée : jusq'à 9 itérations
  - Hypothèse : faire une rotation de l'image pour que les colonnes soient contigüe en mémoire
    - Test : fort peu concluant (prend quasi autant de temps que le traitement lui même)
  - Solution : créer un type MPI 'column' : fait une rotation (données reçu en colonnes), mais sans perte de performance (1 process (envoi à soi-même) = même temps que sans MPI)
- Division colonnes --> Il faut renvoyer toutes les ghosts cells à chaque itération. **C'était long à coder, mais c'est fait!**
  
## OpenMP
- Au départ : grain fin. ```#omp pragma parallel for``` pour toutes les boucles for. Problème : pas de gain (cause probable : création de threads coûteuse)
- Evolution : gros grain.


## Idées stylée 
### Qui marchent
- Manière de diviser le travail entre les process (notamment avec les ghost cells qui se partagent)
- 

### Qui marchent pas 
- Utiliser un scatter pour envoyer toutes les images même si pas mémoire contigüe (mais la différence de 2 pointeurs n'est pas tout le temps définie)
- 


## Observations des données
- Quand n_process * n_thread > 8, on passe sur plusieurs noeuds pour traiter une image et le temps s'effondre
- Avec méthode gros grain : peu d'évolution avec plusieurs threads