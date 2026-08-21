/* -*-comment-start: "//";comment-end:""-*-
 * GNU Mes --- Maxwell Equations of Software
 * Copyright © 2016,2017,2018,2019,2022 Jan (janneke) Nieuwenhuizen <janneke@gnu.org>
 *
 * This file is part of GNU Mes.
 *
 * GNU Mes is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * GNU Mes is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Mes.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <mes/lib.h>
#include <assert.h>

void
__assert_fail (char const *msg, char const *file, unsigned line,
               char const *function)
{
  /* M2-Planet compiles && as bitwise AND and always evaluates both
   * sides.  assert_msg() passes file=function=0; `file && *file` then
   * loads from address 0 and traps before printing the message. */
  eputs ("assert fail: ");
  if (msg != 0)
    eputs (msg);
  eputs ("\n");
  exit (1);
}
