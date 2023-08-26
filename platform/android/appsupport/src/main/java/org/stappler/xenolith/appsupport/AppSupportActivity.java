package org.stappler.xenolith.appsupport;

import android.app.NativeActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

public class AppSupportActivity extends NativeActivity {
	long _nativePointer = 0;
	boolean _hasNotificationPermission = false;

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
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);

		handleActivityResult(_nativePointer, requestCode, resultCode, data);
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
