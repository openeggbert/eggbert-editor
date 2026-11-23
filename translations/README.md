# Translations

`eggbert-editor_<language>.ts` are Qt Linguist catalogues. The program reads them as they are at run time (`src/app/catalog_translator.cpp`), so building needs no Linguist tools. A message whose `<translation>` is marked `type="unfinished"` or is empty is shown in English.

After changing texts in `src/`, refresh the catalogues (this keeps finished translations and adds new messages as unfinished):

```sh
lupdate -no-obsolete -locations none src -ts translations/eggbert-editor_cs.ts
```

Rules checked by the `translation` test: a translation keeps the placeholders of its source (`%1`, `%n`), has a keyboard accelerator (`&`) exactly when the source has one, and ends in `...` exactly when the source does. Messages with `%n` need the three Czech plural forms (one; two to four; five and more).

The names in the game catalogs (tile entries, objects, groups, border families) are plain English in `src/world`; `src/app/catalog_texts.cpp` lists them for the tools under the context `eggbert::Catalog`, and the interface shows them through `catalogText()`. When a catalog gains a name, add it to that list (the `translation` test fails until it is there).

To add a language, add its catalogue here, list the file in `CMakeLists.txt` next to the Czech one, and add the language code to `availableLanguages()` and to the Preferences dialog.
