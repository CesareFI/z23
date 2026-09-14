# Published nonzero-amount reference fixtures

`zip243-transparent.tsv` preserves the two transparent-input records from
[the official ZIP 243 dataset](https://github.com/zcash/zcash-test-vectors/blob/113b3914c79dfe7eb68cb754cd7fea20b75e2e61/test-vectors/json/zip_0243.json),
commit `113b3914c79dfe7eb68cb754cd7fea20b75e2e61`.
The complete original JSON SHA256 is
`3179a8f2773bbc92dd0a509b6281dc36b8b96b41687cd4eae0fbccfea58a64c9`.
The selected TSV SHA256 is
`3767cb2b2a774adde5787e50084d99deb74d859de348dc7b1cf8c0f469a274c3`.
These pin bytes; no publisher-signature claim is made. The complete upstream
MIT license is retained as `zip243-reference.LICENSE`, under the upstream
choice of MIT or Apache-2.0 licensing.

The eight fields are original JSON line, input index, raw hash type, amount,
branch ID, complete wire hex, scriptCode hex, and expected raw digest hex.
Unlike the original Zclassic dataset's displayed uint256 results, these
expected digests compare directly without reversing bytes.

Original lines 5 and 13 sign transparent inputs with NONE and SINGLE flags,
amounts 652655344020909 and 391892287957268, and explicit branch 1991772603.
The other eight records use NOT_AN_INPUT and are outside this fixture's
transparent-input scope. Their proof/script/chain validity is not checked.
These are reference hash comparisons, not Zclassic branch selection or an
assertion that a Zcash transaction is valid on Zclassic.

After checking the complete JSON hash, the selection is reproducible with:

```sh
awk -F '"' '
NR>3 && NF>4 {
    fields=$5; gsub(/[ \t]/,"",fields); n=split(fields,number,",");
    if(n!=6)exit 1;
    if(number[2]<0)next;
    print NR "\t" number[2] "\t" number[3] "\t" number[4] "\t" number[5] "\t" $2 "\t" $4 "\t" $6;
    seen++;
}
END {if(seen!=2)exit 1;}
' zip_0243.json > extracted.tsv
cmp zip243-transparent.tsv extracted.tsv
```

`seed_sighash_vectors` requires both the 130 original Zclassic rows and these
two records to match before emitting any derived expected values. The three
deliberate amount mutations (byte reversal, uint32 truncation, zeroing) pass
all original zero-amount records and fail this added gate. Ten malformed ZIP
inputs also fail without expected output. See [TRANSACTIONS.md](../../docs/TRANSACTIONS.md)
for the generator, projection scope and the remaining wallet-constructor gate.
