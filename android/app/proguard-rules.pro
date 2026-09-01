# Keep JNI entry points and their signatures so the native library can
# resolve Java_com_skifflm_app_SkiffNative_* symbols after shrinking.
-keep class com.skifflm.app.SkiffNative {
    native <methods>;
}
-keep interface com.skifflm.app.SkiffNative$Callback { *; }
-keepattributes Signature,InnerClasses,EnclosingMethod
