/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2020 Gundolf Kiefer
 *
 *  Home2L is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Home2L is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Home2L. If not, see <https://www.gnu.org/licenses/>.
 *
 */


/*
 * This file implements an interface for the "sync2l" synchronization tool
 * to access the Android device's contacts database ("com.android.localphone/PHONE").
 * To enable its functionality,
 *
 * a) the following permissions are required:
 *      <uses-permission android:name="android.permission.READ_CONTACTS" />
 *      <uses-permission android:name="android.permission.WRITE_CONTACTS" />
 *
 * b) an object of this class 'Home2lSync2l' must be instantiated, which usually
 *    happens if 'sync2l = <pipename>' is set in the Home2l main configuration.
 *
 * If you do not know what 'sync2l' is, you should not enable this module.
 * If you do enable it, you should be aware that anybody who can write to the
 * named pipe special file has full access to your address book (typically
 * all members of the group 'home2l' - see environment parameter 'sync2l' /
 * 'system.C').
 */


package org.home2l.app;

import java.util.Set;
import java.util.Map;
import java.util.HashMap;
import java.util.ArrayList;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

import java.io.FileReader;
import java.io.FileWriter;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.FileInputStream;

import android.content.Context;
import android.content.ContentProviderOperation;
import android.database.Cursor;
import android.provider.ContactsContract;
import android.net.Uri;
import android.content.res.AssetFileDescriptor;


public class Home2lSync2l extends Thread {
  static protected final String rawContactAccountType = "com.android.localphone";
  static protected final String rawContactAccountName = "PHONE";
  protected Context context;
  protected String pipeName;



  // ************************* Helpers *********************************


  // Custom class for all kinds of home-brewed exceptions...
  protected static class MyException extends Exception {
    public MyException (String msg) { super (msg); }
  }


  protected static void doAddKeyValue (Map<String, String> map, String key, String value) {
    if (value == null) return;          // make sure that no 'null' strings are written to the map
    String prevValue = map.get (key);
    if (prevValue == null) map.put (key, value);
    else map.put (key, prevValue + ", " + value);
  }


  protected static String mapToLine (Map<String, String> map) {
    Set<String> keySet = map.keySet ();
    String line = "";

    //~ Home2l.logInfo ("### mapToLine ( " + map.toString () + " )");

    // Append known keys in defined order...
    for (String key: new String [] { "N", "TEL", "TEL;TYPE=HOME", "TEL;TYPE=WORK", "TEL;TYPE=CELL", "EMAIL", "EMAIL;TYPE=HOME", "EMAIL;TYPE=WORK", "ADR", "NOTE", "URL" } ) {
      String val = map.get (key);
      if (val != null) {
        line += " " + key + ":\"" + val.replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";
        keySet.remove (key);
      }
    }

    // Append remaining keys (probably, there aren't any)...
    for (String key: keySet)
      line += " " + key + ":\"" + map.get (key).replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";

    // Skip leading space...
    try { line = line.substring (1); } catch (IndexOutOfBoundsException e) {}

    // Done...
    return line;
  }


  protected static Map<String, String> lineToMap (String line) {
    Map<String, String> map = new HashMap<String, String> ();
    Pattern pattern = Pattern.compile ("\\s*(.*?):\"(.*?[^\\\\])\"");
    Matcher matcher = pattern.matcher (line);
    while (matcher.find ()) {
      map.put (matcher.group (1), matcher.group (2));
      //~ Home2l.logInfo ("# (key, val) = ('" + matcher.group (1) + "', '" + matcher.group (2) + "')");
    }
    return map;
  }





  // ************************* Debugging **************************************


  protected void dumpData () {
    Home2l.logInfo ("### dumpData");
    try {
      //~ BufferedWriter writer = new BufferedWriter (new FileWriter (pipeName));
      FileWriter writer = new FileWriter (pipeName);

      Map<String, Map<String, String>> map = new HashMap<String, Map<String, String>> ();

      // Dump RAW_CONTACTS...
      writer.write ("############################## Dumping RAW_CONTACTS ##############################\n");
      Cursor cursor = context.getContentResolver ().query (ContactsContract.RawContacts.CONTENT_URI, null, null, null, null);
      while (cursor.moveToNext ()) {
        //~ String name = cursor.getString(cursor.getColumnIndex (ContactsContract.CommonDataKinds.Phone.DISPLAY_NAME));
        //~ String phoneNumber = cursor.getString(cursor.getColumnIndex (ContactsContract.CommonDataKinds.Phone.NUMBER));
        //~ writer.write ("N='" + name + "' TEL;TYPE=HOME:'" + phoneNumber + "'\n");

        //~ writer.write ("############################## " + uri.toString () + " / " + ContactsContract.CommonDataKinds.Phone.CONTENT_URI + "\n");
        writer.write ("##########\n");
        int columns = cursor.getColumnCount ();
        for (int i = 0; i < columns; i++) {
          writer.write ("  " + cursor.getColumnName (i) + " = '" + cursor.getString (i) + "'\n");
        }
      }


      // Dump DATA...
      Uri uri = ContactsContract.Data.CONTENT_URI;
      cursor = context.getContentResolver ().query (uri, null, null, null, null);
        // ContactsContract.RawContacts.CONTENT_URI
      while (cursor.moveToNext ()) {
        //~ String name = cursor.getString(cursor.getColumnIndex (ContactsContract.CommonDataKinds.Phone.DISPLAY_NAME));
        //~ String phoneNumber = cursor.getString(cursor.getColumnIndex (ContactsContract.CommonDataKinds.Phone.NUMBER));
        //~ writer.write ("N='" + name + "' TEL;TYPE=HOME:'" + phoneNumber + "'\n");

        writer.write ("############################## " + uri.toString () + " / " + ContactsContract.CommonDataKinds.Phone.CONTENT_URI + "\n");
        int columns = cursor.getColumnCount ();
        for (int i = 0; i < columns; i++) {
          writer.write ("  " + cursor.getColumnName (i) + " = '" + cursor.getString (i) + "'\n");
        }
      }
      cursor.close();

      writer.close ();
    }
    catch (IOException e) {
      Home2l.logError ("Sync2l error: " + e.getMessage ());
    }
  }





  // ************************* Write contacts **********************************


  protected void writeContacts () {
    //~ Home2l.logInfo ("### writeContacts");
    try {

      // Iterate over the complete "DATA" database...
      Map<String, Map<String, String>> map = new HashMap<String, Map<String, String>> ();
      Uri uri = ContactsContract.Data.CONTENT_URI;
      Cursor cursor = context.getContentResolver ().query (uri, null, null, null, null);
      while (cursor.moveToNext ()) {

        // Get contact ID and corresponding (key, value) map...
        String rawContactId = cursor.getString (cursor.getColumnIndex (ContactsContract.Data.RAW_CONTACT_ID));
        Map<String, String> cardMap = map.get (rawContactId);
        if (cardMap == null) {
          cardMap = new HashMap<String, String> ();
          map.put (rawContactId, cardMap);
        }

        // Try to identify type and add corresponding field...
        String key;

        //    name...
        String mimeType = cursor.getString (cursor.getColumnIndex (ContactsContract.Data.MIMETYPE));
        if (mimeType.equals (ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE)) {
          String name = cursor.getString (cursor.getColumnIndex (ContactsContract.CommonDataKinds.StructuredName.DISPLAY_NAME));
          // TBD: Adapt name? Combine name from given name, family name etc.?
          doAddKeyValue (cardMap, "N", name);
        }

        //    phone numbers...
        if (mimeType.equals (ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)) {
          switch (cursor.getInt (cursor.getColumnIndex (ContactsContract.CommonDataKinds.Phone.TYPE))) {
            case ContactsContract.CommonDataKinds.Phone.TYPE_HOME:
              key = "TEL;TYPE=HOME"; break;
            case ContactsContract.CommonDataKinds.Phone.TYPE_WORK:
              key = "TEL;TYPE=WORK"; break;
            case ContactsContract.CommonDataKinds.Phone.TYPE_MOBILE:
              key = "TEL;TYPE=CELL"; break;
            case ContactsContract.CommonDataKinds.Phone.TYPE_FAX_WORK:
            case ContactsContract.CommonDataKinds.Phone.TYPE_FAX_HOME:
              key = "TEL;TYPE=FAX"; break;
            default:
              key = "TEL";
          }
          doAddKeyValue (cardMap, key, cursor.getString (cursor.getColumnIndex (ContactsContract.CommonDataKinds.Phone.NUMBER)));
        }

        //    e-mail addresses...
        if (mimeType.equals (ContactsContract.CommonDataKinds.Email.CONTENT_ITEM_TYPE)) {
          switch (cursor.getInt (cursor.getColumnIndex (ContactsContract.CommonDataKinds.Email.TYPE))) {
            case ContactsContract.CommonDataKinds.Email.TYPE_HOME:
              key = "EMAIL;TYPE=HOME"; break;
            case ContactsContract.CommonDataKinds.Email.TYPE_WORK:
              key = "EMAIL;TYPE=WORK"; break;
            default:
              key = "EMAIL";
          }
          doAddKeyValue (cardMap, key, cursor.getString (cursor.getColumnIndex (ContactsContract.CommonDataKinds.Email.ADDRESS)));
        }

        //    address...
        if (mimeType.equals (ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_ITEM_TYPE))
          doAddKeyValue (cardMap, "ADR", cursor.getString (cursor.getColumnIndex (ContactsContract.CommonDataKinds.StructuredPostal.FORMATTED_ADDRESS)));

        //    notes...
        if (mimeType.equals (ContactsContract.CommonDataKinds.Note.CONTENT_ITEM_TYPE))
          doAddKeyValue (cardMap, "NOTE", cursor.getString (cursor.getColumnIndex (ContactsContract.CommonDataKinds.Note.NOTE)));

        //    website...
        if (mimeType.equals (ContactsContract.CommonDataKinds.Website.CONTENT_ITEM_TYPE))
          doAddKeyValue (cardMap, "URL", cursor.getString (cursor.getColumnIndex (ContactsContract.CommonDataKinds.Website.URL)));

        // ... done extracting field.
      }
      cursor.close();

      // Write out map...
      FileWriter writer = new FileWriter (pipeName);
      for (String key: map.keySet ()) {   // loop over all raw contacts
        Map<String, String> cardMap = map.get (key);
        //~ writer.write ("### " + cardMap.toString () + "\n");
        writer.write (mapToLine (cardMap) + "\n");
      }
      writer.close ();
    }
    catch (IOException e) {
      Home2l.logError ("Sync2l error: " + e.getMessage ());
    }
  }





  // ************************* Delete a contact *******************************


  protected void delContact (String idVal, boolean warnIfMissing) {
    // 'idVal' is the value of the identifying key ("N").

    //~ Home2l.logInfo ("# - " + idVal);
    try {

      // Get 'rawContactId'(s)...
      //   ... by querying DATA for the 'StructuredName' entry of the contact to remove.
      Cursor cursor = context.getContentResolver ().query (
          ContactsContract.Data.CONTENT_URI,
          // projection...
          new String [] { ContactsContract.Data.MIMETYPE, ContactsContract.Data.RAW_CONTACT_ID, ContactsContract.CommonDataKinds.StructuredName.DISPLAY_NAME },
          // selection...
          ContactsContract.Data.MIMETYPE + " = ? AND " + ContactsContract.CommonDataKinds.StructuredName.DISPLAY_NAME + " = ?",
          // selection args...
          new String [] { ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE, idVal },
          null,
          null
        );
      if (cursor.getCount () == 0 && warnIfMissing)
        Home2l.logWarning ("Sync2l: Unable to delete non-existing contact N = '" + idVal + "'");
      if (cursor.getCount () > 1)
        Home2l.logWarning ("Sync2l: More than one contact has N = '" + idVal + "' - deleting all of them");

      // Loop over all raw contacts and build remover batch job...
      ArrayList<ContentProviderOperation> ops = new ArrayList<ContentProviderOperation> ();    // list of operations for batch processing
      ContentProviderOperation.Builder op;
      while (cursor.moveToNext ()) {
        String rawContactId = cursor.getString (cursor.getColumnIndex (ContactsContract.Data.RAW_CONTACT_ID));
        //~ Home2l.logInfo ("### deleting '" + idVal + "' / _ID = " + rawContactId);

        // Delete all DATA rows referencing 'rawContactId'...
        ops.add (ContentProviderOperation.newDelete (ContactsContract.Data.CONTENT_URI)
                  .withSelection (ContactsContract.Data.RAW_CONTACT_ID + " = ?", new String [] { rawContactId })
                  .build ());

        // Delete respective RAW_CONTACTS row...
        ops.add (ContentProviderOperation.newDelete (ContactsContract.RawContacts.CONTENT_URI)
                  .withSelection ("_ID = ?", new String [] { rawContactId })
                  .build ());
      }
      context.getContentResolver().applyBatch (ContactsContract.AUTHORITY, ops);
    }
    catch (Exception e) {
      Home2l.logWarning ("Sync2l failed to delete contact '" + idVal + "'");
      e.printStackTrace ();
    }
 }





  // ************************* Set a contact **********************************


  protected void putContact (Map<String, String> map) {
    // "add" and "change" are handled identically.
    ArrayList<ContentProviderOperation> ops = new ArrayList<ContentProviderOperation> ();    // list of operations for batch processing
    ContentProviderOperation.Builder op;
    String val;

    //~ Home2l.logInfo ("# +/= " + map.toString ());

    try {

      // Retrieve ID (Field "N" - display name)...
      String id = map.get ("N");
      if (id == null) throw new MyException ("Contact data set is missing an ID (field 'N')");

      // Try to delete first...
      delContact (id, false);

      // Init a batch of operations to insert the RAW_CONTACTS and mandantory DATA entries...
      //   The following code (and its comments) are based on the Google examples at
      //   https://developer.android.com/guide/topics/providers/contacts-provider.html (2017-08-10).

      /*
       * Create a new raw contact with its account type (server type) and account name
       * (user's account). Remember that the display name is not stored in this row, but in a
       * StructuredName data row. No other data is required.
       */
      op = ContentProviderOperation.newInsert(ContactsContract.RawContacts.CONTENT_URI)
            .withValue(ContactsContract.RawContacts.ACCOUNT_TYPE, rawContactAccountType)
            .withValue(ContactsContract.RawContacts.ACCOUNT_NAME, rawContactAccountName);
      ops.add (op.build());

      /* Create the display name for the new raw contact, as a StructuredName data row.
       *
       *    withValueBackReference sets the value of the first argument to the value of
       *    the ContentProviderResult indexed by the second argument. In this particular
       *    call, the raw contact ID column of the StructuredName data row is set to the
       *    value of the result returned by the first operation, which is the one that
       *    actually adds the raw contact row.
       */
      op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
            .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
            // Set the data row's MIME type to StructuredName...
            .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE)
            // Sets the data row's display name to the name in the UI.
            .withValue (ContactsContract.CommonDataKinds.StructuredName.DISPLAY_NAME, id);
      ops.add (op.build());

      // Add phone numbers...
      if ( (val = map.get ("TEL;TYPE=HOME")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Phone.TYPE, ContactsContract.CommonDataKinds.Phone.TYPE_HOME)
              .withValue (ContactsContract.CommonDataKinds.Phone.NUMBER, val);
        ops.add (op.build());
      }
      if ( (val = map.get ("TEL;TYPE=WORK")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Phone.TYPE, ContactsContract.CommonDataKinds.Phone.TYPE_WORK)
              .withValue (ContactsContract.CommonDataKinds.Phone.NUMBER, val);
        ops.add (op.build());
      }
      if ( (val = map.get ("TEL;TYPE=CELL")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Phone.TYPE, ContactsContract.CommonDataKinds.Phone.TYPE_MOBILE)
              .withValue (ContactsContract.CommonDataKinds.Phone.NUMBER, val);
        ops.add (op.build());
      }
      if ( (val = map.get ("TEL;TYPE=FAX")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Phone.TYPE, ContactsContract.CommonDataKinds.Phone.TYPE_FAX_WORK)
              .withValue (ContactsContract.CommonDataKinds.Phone.NUMBER, val);
        ops.add (op.build());
      }
      if ( (val = map.get ("TEL")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Phone.TYPE, ContactsContract.CommonDataKinds.Phone.TYPE_MAIN)
              .withValue (ContactsContract.CommonDataKinds.Phone.NUMBER, val);
        ops.add (op.build());
      }

      // Add e-mail addresses...
      if ( (val = map.get ("EMAIL;TYPE=HOME")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Email.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Email.TYPE, ContactsContract.CommonDataKinds.Email.TYPE_HOME)
              .withValue (ContactsContract.CommonDataKinds.Email.ADDRESS, val);
        ops.add (op.build());
      }
      if ( (val = map.get ("EMAIL;TYPE=WORK")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Email.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Email.TYPE, ContactsContract.CommonDataKinds.Email.TYPE_WORK)
              .withValue (ContactsContract.CommonDataKinds.Email.ADDRESS, val);
        ops.add (op.build());
      }
      if ( (val = map.get ("EMAIL")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Email.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Email.TYPE, ContactsContract.CommonDataKinds.Email.TYPE_OTHER)
              .withValue (ContactsContract.CommonDataKinds.Email.ADDRESS, val);
        ops.add (op.build());
      }

      // Add postal address...
      if ( (val = map.get ("ADR")) != null) {
        // TBD: The complete address is written into the FORMATTED_ADDRESS field. Should be split it an
        //      write to STREET, CITY, ... fields. Or does Android do this for us?
        //      [https://developer.android.com/reference/android/provider/ContactsContract.CommonDataKinds.StructuredPostal.html]
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.StructuredPostal.TYPE, ContactsContract.CommonDataKinds.StructuredPostal.TYPE_OTHER)
              .withValue (ContactsContract.CommonDataKinds.StructuredPostal.FORMATTED_ADDRESS, val);
        ops.add (op.build());
      }

      // Add notes...
      if ( (val = map.get ("NOTE")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Note.CONTENT_ITEM_TYPE)
              // (no 'TYPE' field)
              .withValue (ContactsContract.CommonDataKinds.Note.NOTE, val);
        ops.add (op.build());
      }

      // Add website...
      if ( (val = map.get ("URL")) != null) {
        op = ContentProviderOperation.newInsert (ContactsContract.Data.CONTENT_URI)
              .withValueBackReference (ContactsContract.Data.RAW_CONTACT_ID, 0)
              .withValue (ContactsContract.Data.MIMETYPE, ContactsContract.CommonDataKinds.Website.CONTENT_ITEM_TYPE)
              .withValue (ContactsContract.CommonDataKinds.Website.TYPE, ContactsContract.CommonDataKinds.Website.TYPE_OTHER)
              .withValue (ContactsContract.CommonDataKinds.Website.URL, val);
        ops.add (op.build());
      }

      // Complete and submit batch job...
      context.getContentResolver().applyBatch (ContactsContract.AUTHORITY, ops);
    }
    catch (Exception e) {
      Home2l.logWarning ("Sync2l failed to add/change contact: " + map.toString ());
      e.printStackTrace ();
    }
  }





  // ************************* Top-Level **************************************


  // Constructor...
  public Home2lSync2l (Context _context, String _pipeName) {
    context = _context;
    pipeName = _pipeName;
    start ();
  }


  // Destructor...
  public void done () {
    //~ logInfo ("### Interrupting Thread...");
    try {
      interrupt ();
    } catch (Exception e) {}
    //~ logInfo ("### Joining Thread...");
    try {
      join ();
    } catch (InterruptedException e) {}
    //~ logInfo ("### Thread shut down.");
  }


  // Main thread routine...
  public void run () {
    //~ Home2l.logInfo ("Home2lSync2l thread started");

    while (!interrupted ()) {
      BufferedReader reader;
      String line;

      try {
        reader = new BufferedReader (new FileReader (pipeName));
        while ( (line = reader.readLine ()) != null) {
          line = line.trim ();
          //~ Home2l.logInfo ("### received: '" + line + "'");
          if (!line.isEmpty ()) switch (line.charAt (0)) {
            case '#':
              // comment: ignore line...
              break;
            case '?':
              // query address book...
              reader.close ();      // close pipe to use it in the opposite direction now
              writeContacts ();
              reader = new BufferedReader (new FileReader (pipeName));  // re-open pipe in the reading direction
              break;
            case '-':
              // delete contact...
              delContact (line.substring (1).trim (), true);
              break;
            case '+':
            case '=':
              // add or change contact...
              putContact (lineToMap (line.substring (1)));
              break;
            default:
              // error
              Home2l.logWarning ("Sync2l received illegal line: " + line);
          }
        }
        reader.close ();
      }
      catch (IOException e) {
        Home2l.logError ("Sync2l error: " + e.getMessage ());
        break;
      }
    }
  }
}


