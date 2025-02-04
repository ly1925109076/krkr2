/**
 * 吉里吉里クラスの取得
 * @param className 吉里吉里クラス名指定(文字列)
 * @param ... 継承している親クラスを列挙
 * @return squirrelクラス
 *
 * 吉里吉里のクラスを squirrelクラスとして取得します。
 * このクラスは継承可能です。
 *
 * ※吉里吉里側で親クラス情報を参照生成できないため、
 * 親クラスが継承しているクラスの名前をすべて手動で列挙する必要があります。
 * またこの機能で作成した吉里吉里クラスのインスタンスが吉里吉里側から
 * 返される場合は、squirrel のクラスでのインスタンスとしてラッピングされます。
 */
function createTJSClass(className, ...);
