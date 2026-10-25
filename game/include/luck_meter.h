#ifndef GM_LUCK_METER_H
#define GM_LUCK_METER_H
/* Pinned getLuckString; ordinary party luck is clamped to 0..14. */
static inline const char* luck_grade(int luck) {
  static const char* const grades[]={"D","C","C+","B-","B","B+","A-","A","A+","A++","S","S+","SS","SS+","SSS"};
  return grades[luck<0?0:luck>14?14:luck];
}
#endif
