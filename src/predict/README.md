# predict

Sentence-length spelling and next-word ranking for the Speak page. The query is the current sentence, not a document.

| File | Role |
|------|------|
| `WordPredictor.h` | Query, hits, fixture, file load, `fromPhrase` |
| `PredictModel.cpp` | Trie build, suggest, personal counts |
| `PredictBeam.cpp` | Edit beam on the lexicon trie, phonetic fallback |
| `PredictScore.cpp` | Trigram / anchor / cache mixture, composer key costs |
| `PredictBlob.cpp` | `GZPR` model file and `GZPU` user counts |
| `PredictTrain.cpp` | Build a model from one sentence per line |

`ComposeUi` calls `suggest` when the phrase changes and stamps `pred_0`… on the `predict` row above the phrase. Gaze samples do not query the model.

Score is a noisy channel: log of an interpolated word probability, minus an edit cost. The trigram covers the last two words. Up to four content words earlier in the sentence (closed-class words dropped) pull related candidates through embeddings and PMI triggers. A typed lexicon word is replaced only when the alternative is about 100 times more likely after the edit cost. A longer word that continues the token under the caret stays in the list. A finished word is held to that same margin.

Keyboard costs use the letter positions in `resources/layouts/compose.xml`. A neighbor-key slip, a missing letter, or an adjacent transposition stays in the beam. A far-key substitution does not.

The shipped file is `resources/predict/model.bin`. Sentence lines in `starter-sentences.txt` train the trigram and the anchors. `word-ranks.txt` is a usage-ranked dictionary (Leipzig English news, CC BY 4.0) merged into the same lexicon so a prefix can complete words the sample sentences never used. Sentence words keep their phrase statistics and supply next-word suggestions. Dictionary-only words are ranked by usage count for completion and spelling.

```text
build/BuildPredictModel.exe resources/predict/starter-sentences.txt resources/predict/word-ranks.txt resources/predict/model.bin
```

Version 2 stores probabilities as float32. Each word records whether it occurred in the sentences. Embeddings are stored only for words that have one, and the trie is compiled when the file loads. Replace the blob with a larger one from the same tool; the Speak page does not change. Personal accepts land in `%AppData%\Gazer\predict-user.bin`.

Tests: `GazerPredictTests`.

## Next character

`CharPrior` is an order-2 character model. The shipped file is `resources/predict/charprior.bin` (`GZCH`), written beside `model.bin` by `BuildPredictModel`. Personal counts are `%AppData%\Gazer\char-prior-user.bin` (`GZCU`). For a context with `n` personal observations, those counts replace the shipped estimate by `n / (n + 40)`. A context that has never been seen backs off to the shorter one; a context that has been seen keeps an unseen next at zero.

`WordPredictor::nextCharMass` reads the lexicon and the personal word counts. A word equal to the prefix puts its mass on space. A longer word puts its mass on the next character. `TypingContext` mixes the two once per committed character: 0.45 character prior and 0.55 dictionary when the dictionary has mass. It is the only reader and writer of `char-prior-user.bin` and `predict-user.bin`.

A sentence is counted in source order. Whitespace collapses to one space, and a comma stays before that space. A word character is a letter, digit, apostrophe, or hyphen, the same test as `normWord`, so `don't` and `cat2` are one token when a key stream closes them.

Speak learns from the phrase. Restoring a phrase after a name edit adopts that string without counting it again. Every other keyboard learns from the keys Gazer sends, and one backspace undoes that count. A finished word of two or more letters is saved in the same personal dictionary as an accepted suggestion. `KeyGravity` uses the mixture to move the rapid-dwell target toward likely characters and to scale the rapid sequence. Gaze samples read the cached distribution and do not query the model.
