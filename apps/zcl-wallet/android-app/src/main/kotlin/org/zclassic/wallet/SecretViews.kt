// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.os.Build
import android.os.Parcelable
import android.util.SparseArray
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

internal fun View.protectSecretView() {
    isSaveEnabled = false
    importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
    importantForContentCapture = View.IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS
    filterTouchesWhenObscured = true
    if (Build.VERSION.SDK_INT >= 34) setAccessibilityDataSensitive(View.ACCESSIBILITY_DATA_SENSITIVE_YES)
}

/** Takes ownership of the supplied char array. Never creates a secret String.
 * Framework/rendering copies cannot all be erased; see the custody threat model.
 */
internal class RecoveryWordsView(context: Context) : TextView(context) {
    private var words: CharArray? = null

    init {
        protectSecretView()
        setTextIsSelectable(false)
        isLongClickable = false
        textSize = 21f
        setLineSpacing(8f, 1.1f)
    }

    fun show(ownedWords: CharArray) {
        try {
            clearSecret()
            words = ownedWords
            require(ownedWords.size <= 215) { "Recovery display exceeds its bound" }
            setText(ownedWords, 0, ownedWords.size)
            visibility = VISIBLE
        } catch (problem: Throwable) {
            // Ownership starts at entry, even if clearing the old display
            // fails before the incoming array can become this view's field.
            ownedWords.fill('\u0000')
            try { clearSecret() }
            catch (cleanup: Throwable) { if (cleanup !== problem) problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    fun clearSecret() {
        val previous = words
        words = null
        // TextView may retain its own copy if clearing fails. Conceal it before
        // calling the framework; only a complete later show may reveal it.
        try { visibility = INVISIBLE; text = "" }
        finally { previous?.fill('\u0000') }
    }

    override fun dispatchSaveInstanceState(container: SparseArray<Parcelable>) = Unit
    override fun dispatchRestoreInstanceState(container: SparseArray<Parcelable>) { clearSecret() }

    override fun onDetachedFromWindow() {
        clearSecret()
        super.onDetachedFromWindow()
    }
}

/** In-app recovery keyboard: no EditText, input connection, clipboard or IME.
 * C still validates the complete canonical phrase/checksum. This view only
 * collects a bounded sequence of the keys that are visible on its keyboard.
 */
internal class RecoveryInputView(context: Context) : LinearLayout(context) {
    private val characters = CharArray(215)
    private var length = 0
    private val preview = TextView(context).apply {
        protectSecretView()
        setTextIsSelectable(false)
        isLongClickable = false
        textSize = 18f
        minHeight = (112 * resources.displayMetrics.density).toInt()
    }

    init {
        orientation = VERTICAL
        protectSecretView()
        addView(preview)
        for (letters in arrayOf("qwertyuiop", "asdfghjkl", "zxcvbnm")) {
            val row = LinearLayout(context)
            for (letter in letters) row.addView(key(letter.toString()) { append(letter) })
            addView(row)
        }
        val controls = LinearLayout(context)
        controls.addView(key(context.getString(R.string.recovery_space)) { append(' ') })
        controls.addView(key(context.getString(R.string.recovery_delete)) { deleteLast() })
        controls.addView(key(context.getString(R.string.recovery_clear)) { clearSecret() })
        addView(controls)
    }

    private fun key(label: String, action: () -> Unit): Button = Button(context).apply {
        text = label // Public keyboard labels only.
        minWidth = 0
        minimumWidth = 0
        setPadding(0, 0, 0, 0)
        layoutParams = LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
        protectSecretView()
        setOnClickListener { action() }
    }

    internal fun append(character: Char) {
        if (character !in 'a'..'z' && character != ' ') return
        if (length == characters.size) return
        characters[length++] = character
        updatePreview()
    }

    private fun deleteLast() {
        if (length == 0) return
        characters[--length] = '\u0000'
        updatePreview()
    }

    private fun updatePreview() {
        try {
            preview.visibility = INVISIBLE
            preview.setText(characters, 0, length)
            preview.visibility = VISIBLE
        }
        catch (problem: Throwable) {
            try { clearSecret() }
            catch (cleanup: Throwable) { if (cleanup !== problem) problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    fun takeInput(): CharArray {
        val count = length
        length = 0
        try {
            // Clear rendering before allocating a transferred copy. A failed
            // clear cannot strand that copy, and allocation failure still wipes
            // the original buffer through finally.
            preview.visibility = INVISIBLE
            preview.text = ""
            return characters.copyOf(count)
        } finally { characters.fill('\u0000') }
    }

    fun clearSecret() {
        length = 0
        try { preview.visibility = INVISIBLE; preview.text = "" }
        finally { characters.fill('\u0000') }
    }

    override fun dispatchSaveInstanceState(container: SparseArray<Parcelable>) = Unit
    override fun dispatchRestoreInstanceState(container: SparseArray<Parcelable>) { clearSecret() }

    override fun onDetachedFromWindow() {
        clearSecret()
        super.onDetachedFromWindow()
    }
}
