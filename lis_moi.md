# Préambule :

	Bonjour et merci d'avoir téléchargé cet utilitaire conçu et programmé entièrement par Téo Pisenti
Ce programme est désormais dans un état stable avec la majorité des fonctionnalités prévues qui ont été implémentées. Toutefois, il se peut que certains bogues subsistes, je vous en serai gré de me tenir au courant de telles découvertes.

	ce programme porte le nom de "kbjoy", pour "kb to joy" = "keyboard to joystick". Il vous permettra en effet de remapper (rediriger) les entrées indépendantes effectuées sur plusieurs claviers connectés à un même pc sur des manettes virtuelles.

	En effet, les entrées claviers en temps normal traitées de façon identiques quel que soit le clavier utilisé, ce qui empêche l'utilisation de plusieurs claviers pour plusieurs joueurs en jeu en proposant un layout identique pour chacun d'entre eux. (de plus, le joueur 2 en appuyant sur les touches de son clavier pouvait déclencher les actions du joueur 1 pour l'handicaper).

	C'est donc par souci d'équité, et pour facilité la mise en place de jeux multijoueurs à écran partagé que j'ai développé ce programme (avoir plusieurs claviers à disposition étant plus courant qu'avoir de nombreuses manettes).

	Le but enfin était de pouvoir fournir non seulement un programme de mapping mais également une API permettant de manipuler facilement ce programme à travers des applications tierces (il est prévu de réaliser prochainement une interface graphique). Le tout dans le respect le plus strict de l'arborescence de fichiers et des rêgles de sécurité sous Linux (l'application ne disposera jamais des droits roots, et votre utilisateur doit juste appartenir au groupe kbjoy et non root pour pouvoir l'utiliser).

Le code est entièrement commenté, malgré mon anglais douteux ;)

# Installation :

	Il est très simple d'installer ce programme, il suffit d'ouvrir un terminal dans le dossier qui contient ce fichier texte, et d'exécuter le script d'installation "Install.sh" avec les droits root qui se chargera de compiler et copier les fichiers aux bons endroits.
`$ sudo ./Install.sh`

	Une fois le script terminé, vous serez invités à redémarrer votre ordinateur.

	Pour le moment, le script de désinstallation n'a pas été réalisé, mais vous devriez facilement retrouver les fichiers créés ou modifiés en lisant le script d'installation.

# Post-Installation :

	L'installation va notamment créer un nouveau service systemd nommé "kbjoy". Toutefois celui-ci n'est pas actif pour le moment, pour le lancer utilisez la commande:
`$ sudo systemctl start kbjoy`

	Vous pouvez l'ajouter à la liste des services automatiquement lancés au démarrage de l'ordinateur avec la commande:
`$ sudo systemctl enable kbjoy` (vous devrez redémarrer l'ordinateur pour que cela prenne effet)

	Le script d'installation va créer un nouvel utilisateur système (un utilisateur qui n'a pas de dossier personnel et qui ne peut pas ouvrir de session) afin de donner des droits très limités au service kbjoy, et nommé "kbjoy". Cet utilisateur n'a accès qu'aux groupes "input" qui permet de lire et bloquer les périphériques d'entrée, ainsi qu'au groupe "uinput" créé spécialement par le script. Ce dernier ne donne droit qu'a l'écriture du fichier "/dev/uinput" indispensable pour créer des manettes virtuelles. (ce fichier est ajouté au groupe uinput au démarrage de l'ordinateur par l'ajout de deux lignes dans "/etc/rc.local" lors de l'installation.

	!! Cet utilisateur possède son propre groupe de fichiers. Cela signifie qu'il est obligatoire d'appartenir au groupe "kbjoy" pour avoir le droit d'envoyer des commandes au service kbjoy. ** Vous devez ajouter votre utilisateur au groupe kbjoy avec la commande:**
`$ sudo adduser user kbjoy` (vous devrez au minimum redémarrer la session pour que ce changement prenne effet)

# Utilisation :
	Tout d'abord assurez d'avoir les droits suffisants (voir précédemment) et que le service kbjoy est actif (vous pouvez utiliser la commande `sudo systemctl status kbjoy`).

Dans un premier temps je vous propose de lancer la commande dans un terminal utilisateur la commande suivante:
`$ kbjoy-client ping`
Cela devrait renvoyer le message "receive 13 ping success", si vous avez une erreur, veillez vérifier les étapes précédentes.
(cette commande sert uniquement à tester si tout va bien avant de lancer d'autres commandes, aussi elle est plutôt inutile)

	L'installation par défaut vous propose deux utilitaires en ligne de commande pour manipuler le service kbjoy:
		- kbjoy-connect -> cela permet de voir les réponses renvoyées à l'issue de chaque commande
		- kbjoy-client -> cela permet de lancer les commandes (toutefois sans l'utilisation de kbjoy-connect vous ne pouvez pas savoir si elles ont abouti, sauf exception de la commande "ping")

## Premier utilitaire: kbjoy-connect
	Je vous propose maintenant de lancer le premier utilitaire avec la commande:
`$ kbjoy-connect`

voici un exemple de la sortie de ce programme:

sent: connect
receive 9 accept:
connection ID: 3
About to extract fd
Extracted fd 4
pid: 3469 
testlock
file locked
timeout set 
skipped: list-start
skipped: /dev/input/event0
skipped: list-end
skipped: begin:
skipped: begin:
skipped: begin:
timeout removed 
recv: list-start
recv: /dev/input/event0
recv: list-end

	Vous pouvez ignorer tous les messages avant "timeout set". L'ensemble des messages précédent "timeout removed" et commençant par "skipped: " correspondent aux messages en attentes qui ont été émis par le service kbjoy AVANT le lancement de kbjoy-connect. L'ensemble des messages après "timeout removed" et commençant par "recv: " sont ceux ayant été émis PENDANT l'éxécution de kbjoy-connect. Vous pourrez l'arrêter à tout moment avec le raccourci "control+C".

	REMARQUE: Ce programme demande la connection en écoute au service kbjoy. Il ne peut y avoir qu'un seul client en écoute à la fois. Cela signifie que vous ne pourrez lancer 2 fois kbjoy-connect en même temps, car le service refusera la seconde connexion.

## Deuxième utilitaire: kbjoy-client
	Lançons maintenant dans un second terminal l'utilitaire "kbjoy-client" (je vous conseille de laisser kbjoy-connect tourner afin de voir si vos commandes ont fonctionné). Cet utilitaire à pour unique rôle d'envoyer nos commandes au service kbjoy. Il s'exécute de cette façon:
`$ kbjoy-client [commande à exécuter] [arguments séparés par des espaces]

### list
	Cette commande permet de lister tous les claviers connectés actuellement à l'ordinateur. La sortie dans kbjoy-connect est la suivante:
recv: list-start
recv: /dev/input/event0
recv: /dev/input/event6
recv: list-end

le début et la fin de la liste des claviers sont identifiés respectivement par "list-start" et "list-end". Le chemin d'accès de tous les claviers trouvés est donné entre ces deux délimitateurs, soit dans l'exemple "/dev/input/event0" et "/dev/input/event6".

### trigger [triggerKey | stop]
	La commande trigger est une version améliorée de la commande list. Elle va lancer un nouveau processus qui listera les claviers présents comme la commande info, mais va également renvoyer le chemin d'accès, en temps réel du clavier dont la touche demandée aura été pressée ou relâchée.

	Elle prend un argument "triggerKey" qui correspond au code d'une touche de clavier tel que "KEY_A" qui correspond à la touche "A" du clavier QWERTY (donc "Q" en AZERTY). Pour plus d'info, veuillez vous référrer au fichier input-event-codes.h.

**example:** `$ kbjoy-client trigger KEY_1` va renvoyer un message pour chaque clavier quand on appuie sur la touche '1'.

**REMARQUES:** L'ensemble des noms de touches n'a pas été implémenté. Aussi, vous pouvez spécifier un code directement en précédent le numéro du caractère '\'. Ainsi "\28" correspond à la touche "ENTREE".
	De plus, les claviers de l'ordinateur ne sont pas listés de nouveau durant l'execution du processus. Par conséquent, si un clavier supplémentaire est branché, il faudra relancer le processus pour qu'il soit pris en compte.

	Pour stopper l'exécution du processus (et donc le flot de message dans kbjoy-connect) il suffit de lancer:
`$ kbjoy-client trigger stop`

Les messages:
recv: kbjoy-trigger good
recv: trigger process exited

permettent de savoir quand le processus démarre et quand il s'arrête.

### add / del [gamepad number]
	On enchaine avec des commandes très importantes: add et del. Elles permettent respectivement (comme leur nom l'indique) d'ajouter ou de supprimer un controlleur virtuel. Le controlleur virtuel à ce moment là n'execute aucune action, mais devient visible dans les jeux. Ce n'est qu'à la destruction du controlleur virtuel que le jeu considèrera le joueur comme déconnecté. Cela signifie, qu'il est considéré comme connecté, même si aucun clavier n'est assigné à sa manette.

	L'argument *gamepad number* correspond au numéro de la manette crée ou détruite. Il doit être compris entre 1 et le numéro du dernier joueur (8 par défaut pour 8 joueurs simultanés, il peut être modifié dans le code).
Les sorties dans kbjoy-connect sont comme ceci:
recv: kbjoy Player 1 added 		-> création d'un nouveau controlleur virtuel 
recv: kbjoy Player 1 removed		-> destruction d'un controlleur virtuel existant

Le controlleur aura le nom "kbjoy Player X" ou X correspond au numéro du joueur (1 dans l'exemple).

### start [gamepad number] [keyboard path] [optional: config path] / stop [gamepad number]
	Voici les deux dernières commandes disponibles permettant d'assigner, ou de dé-assigner un clavier d'un controlleur virtuel. C'est après l'execution de la commande *start* que vous pourrez enfin jouer avec votre clavier dans vos jeux favoris (ou autre logiciel qui accepte des entrées de manette).
	- L'argument *gamepad number* correspond au controlleur virtuel sur lequel sera (dé-)attaché le clavier.
	- L'argument *keyboard path* correspond au chemin d'accès du fichier du clavier à (dé-)attacher. C'est celui obtenu par les commandes *list* ou *trigger*
	- L'argument *config path* correspond au chemin d'accès ABSOLU ("./" ou "~/" ne fonctionnent pas) du fichier contenant la configuration des touches du clavier et de la disposition de la future manette. Si ce paramètre est omi, ce sera la configuration par défaut qui sera utilisée: **/etc/kbjoy-default.map**. Vous pourrez en apprendre plus sur la configuration du clavier dans ce fichier, ou dans les différents exemples dans le dossier *configs* (dans le même dossier que ce documents que vous êtes en train de lire)

**REMARQUE:** la commande *stop* ne nécessite qu'un seul argument tandis que start en nécessite minimum 2.

**REMARQUE 2:** il n'est pas possible de modifier la configuration d'un clavier attaché à un controlleur virtuel. Pour cela, il suffit de le détacher avec la commande stop et de relancer la commande start. Le controlleur virtuel n'étant pas modifié, cette action est invisible pour le jeu.

### Raccourcis clavier IMPORTANTS
	Une fois le clavier attaché à un controlleur virtuel, celui-ci va se retrouver en **mode manette**. Cela signifie, que les touches spécifiées par le fichier de configuration seront transmises à la manette virtuelle et toutes les autres ignorées. Vous ne pourrez donc plus entrer de texte.

**IL EST IMPERATIF DE SAVOIR COMMENT QUITTER LE MODE MANETTE AU RISQUE D'ETRE BLOQUE !!!**

- Vous pouvez bien-sûr débrancher le clavier, ce qui va automatiquement le détacher et détruire son controlleur virtuel.
- Le même mécanisme se déclenchera si vous utiliser le **raccourci de secours: CONTROL + ALT + SUPPR** accessible à tout moment.

- Toutefois, de façon plus souple, il est possible de passer en **mode clavier** (et alternativement repasser en mode manette) en utilisant le raccourci: **SUPER (touche windows) + ECHAP**. Le controlleur virtuel sera toujours connecté, mais mis en pause et vous pourrez saisir du texte à nouveau jusqu'a que vous refassiez le raccourci.

Voilà, ce document est maintenant terminé, j'espère que cette application vous sera utile (je rappelle qu'elle n'est fonctionnelle que sous linux et non sur Windows, Mac-OS)
