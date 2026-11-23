# Translations

`eggbert-editor_<language>.ts` are Qt Linguist catalogues. The program reads them as they are at run time (`src/app/catalog_translator.cpp`), so building needs no Linguist tools. A message whose `<translation>` is marked `type="unfinished"` or is empty is shown in English.

After changing texts in `src/`, refresh the catalogues (this keeps finished translations and adds new messages as unfinished):

```sh
lupdate -no-obsolete -locations none src -ts translations/eggbert-editor_cs.ts
```

Rules checked by the `translation` test: a translation keeps the placeholders of its source (`%1`, `%n`), has a keyboard accelerator (`&`) exactly when the source has one, and ends in `...` exactly when the source does. Messages with `%n` need the three Czech plural forms (one; two to four; five and more).

The names in the game catalogs (tile entries, objects, groups, border families) are plain English in `src/world`; `src/app/catalog_texts.cpp` lists them for the tools under the context `eggbert::Catalog`, and the interface shows them through `catalogText()`. When a catalog gains a name, add it to that list (the `translation` test fails until it is there).

World Doctor's findings are composed in `src/world/doctor.cpp` from patterns (`%1` to `%9`) and arguments; `src/app/doctor_texts.cpp` lists the patterns under the context `eggbert::Doctor` and `findingText()` fills the translated pattern with the same arguments. When a rule gains a sentence, add its pattern to both lists (the `doctor` and `translation` tests fail until it is there).

Other sentences composed in `src/world` are shown through `coreText()` (`src/app/core_texts.cpp`): a sentence is looked up by its pattern, which is the sentence with every number replaced by `%1`, `%2`, … in order, under the context `eggbert::Core`; one that is not listed there is shown in English. The encyclopedia's facts are parts listed in `src/world/encyclopedia.cpp` and marked in `src/app/doctor_texts.cpp` (`eggbert::Encyclopedia`); conversion notes likewise in `src/app/conversion_texts.cpp`.

The user guide has one file per language (`docs/user-guide.md`, `docs/user-guide.cs.md`); the catalogue translates the file name `user-guide.md` to the one Help should show. The `translation` test keeps the two guides' headings, list items and line counts in step, so a paragraph added to one must be added to the other.

To add a language, add its catalogue here, list the file in `CMakeLists.txt` next to the Czech one, and add the language code to `availableLanguages()` and to the Preferences dialog.
