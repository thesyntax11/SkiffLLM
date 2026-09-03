# Keep JNI entry points and their signatures so the native library can
# resolve Java_com_llm_app_SkiffNative_* symbols after shrinking.
-keep class com.llm.app.SkiffNative {
    native <methods>;
}
-keep interface com.llm.app.SkiffNative$Callback { *; }
-keepattributes Signature,InnerClasses,EnclosingMethod
