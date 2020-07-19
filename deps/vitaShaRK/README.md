# vitaShaRK
**vita** **Sha**ders **R**untime **K**ompiler is a runtime shader compiler library for PSVITA/PSTV using the SceShaccCg module contained inside the PSM runtime.

# Build Instructions
In order to build vitaShaRK, you'll first need to build SceShaccCg stubs. This is a full list of commands you can use to install this library and the required stubs:
```
vita-libs-gen SceShaccCg.yml build
cd build
make install
cd ..
cp shacccg.h $VITASDK/arm-vita-eabi/psp2/shacccg.h
make install
```


# Credits

**frangarcj** for the original vita2d shader compiler source used as base to build up this library.
