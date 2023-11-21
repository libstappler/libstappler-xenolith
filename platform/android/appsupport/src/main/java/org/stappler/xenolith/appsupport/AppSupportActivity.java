package org.stappler.xenolith.appsupport;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.text.InputType;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;

public class AppSupportActivity extends NativeActivity {
	long _nativePointer = 0;
	boolean _hasNotificationPermission = false;
	FrameLayout _frameLayout = null;
	View _contentView = null;
	TextInputWrapper _input = null;

	@Override
	protected void onStart() {
		super.onStart();
	}

	@Override
	protected void onResume() {
		super.onResume();
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();

		_nativePointer = 0;
	}

	@Override
	public View onCreateView(View parent, String name, Context context, AttributeSet attrs) {
		return super.onCreateView(parent, name, context, attrs);
	}

	@Override
	public View onCreateView(String name, Context context, AttributeSet attrs) {
		return super.onCreateView(name, context, attrs);
	}

	@Override
	public void setContentView(View view) {
		ViewGroup.LayoutParams framelayout_params =
				new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
						ViewGroup.LayoutParams.MATCH_PARENT);
		_frameLayout = new FrameLayout(this);
		_frameLayout.setLayoutParams(framelayout_params);

		ViewGroup.LayoutParams edittext_layout_params =
				new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
						ViewGroup.LayoutParams.WRAP_CONTENT);
		TextInputEditText editText = new TextInputEditText(this);
		editText.setLayoutParams(edittext_layout_params);

		_frameLayout.addView(editText);
		_contentView = view;
		// ...add to FrameLayout
		_frameLayout.addView(view);

		_input = new TextInputWrapper(this, editText);

		super.setContentView(_frameLayout);
	}

	public View getContentView() {
		return _contentView;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);

		handleActivityResult(_nativePointer, requestCode, resultCode, data);
	}
	public void runInput(String text, int cursorStart, int cursorLen, int type) {
		_input.runInput(text, cursorStart, cursorLen, type);
	}

	public void updateInput(String text, int cursorStart, int cursorLen, int type) {
		_input.updateInput(text, cursorStart, cursorLen, type);
	}

	public void updateCursor(int cursorStart, int cursorLen) {
		_input.updateCursor(cursorStart, cursorLen);
	}

	public void cancelInput() {
		_input.cancelInput();
	}

	/*private void askNotificationPermission() {
		// This is only necessary for API level >= 33 (TIRAMISU)
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
			if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
					PackageManager.PERMISSION_GRANTED) {
				// Directly ask for the permission
				Intent intent = _activityResultContract.createIntent(this, Manifest.permission.POST_NOTIFICATIONS);
				startActivityForResult(intent, PERMISSION_REQUEST_ID);
			} else {
				_hasNotificationPermission = true;
			}
		} else {
			_hasNotificationPermission = true;
		}
	}*/

	protected void openURL( String inURL ) {
		Intent browse = new Intent( Intent.ACTION_VIEW , Uri.parse( inURL ) );
		browse.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity( browse );
	}

	public long getNativePointer() {
		return _nativePointer;
	}

	protected void setNativePointer(long nativePointer) {
		_nativePointer = nativePointer;
	}

	protected boolean isEmulator() {
		return Build.PRODUCT.equals("sdk") || Build.PRODUCT.contains("_sdk") || Build.PRODUCT.contains("sdk_");
	}

	protected native void handleActivityResult(long nativePointer, int requestCode, int resultCode, Intent data);

	static {
		System.loadLibrary("application");
	}
}
