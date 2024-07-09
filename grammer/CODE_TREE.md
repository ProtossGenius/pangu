## Normal
`a = b * c + d / e`
of cause [*,/] > [+,-] > [=]
1. a
2. (=, a, null)
3. (=, a, b)
// cause [*] > [=], * as a subtree
4. (=, a, (*, b, null))
5. (=, a, (*, b, c))
6. (=, a, (*, b, c))
// cause [+] < [*] and [+] > [=], + as [=]'s subtree
7. (=, a, (+, (*, b, c), null))
9. (=, a, (+, (*, b, c), d))
10. (=, a, (+, (*, b, c), d))
// case [+] < [*], * as [+]'s subtree.