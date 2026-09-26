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
