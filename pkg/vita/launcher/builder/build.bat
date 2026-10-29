@set /p title="Insert bubble name: "
@set /p rom="Insert rom fullpath: "
@set /p core="Insert core fullpath: "
@set /p id="Insert bubble title ID (9 characters [NOTE: Only UPPERCASE letters or numbers]): "
@echo|set /p="%core%"> "contents/core.txt"
@echo|set /p="%rom%"> "contents/rom.txt"
vita-mksfoex -s TITLE_ID=%id% "%title%" param.sfo
vita-pack-vpk -s param.sfo -b eboot.bin "%title%.vpk" -a contents/icon0.png=sce_sys/icon0.png -a contents/bg.png=sce_sys/livearea/contents/bg.png -a contents/startup.png=sce_sys/livearea/contents/startup.png -a contents/template.xml=sce_sys/livearea/contents/template.xml -a contents/core.txt=core.txt -a contents/rom.txt=rom.txt