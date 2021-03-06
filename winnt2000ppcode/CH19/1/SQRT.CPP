====================== Listing 7 ======================
/* Compute the greatest integer less than or equal
   to the square root of lint.  From "Factorization
   and Primality Testing", Bressoud
   ------------------------------------------------ */
LargeInt sqrt(const LargeInt& lint) {
   LargeInt a = lint;
   LargeInt b = (lint + one) / two;

   while (b < a) {
      a = b;
      b = (a * a + lint) / (two * a);
   }
   return a;
}
